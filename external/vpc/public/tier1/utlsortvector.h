// Copyright Valve Corporation, All rights reserved.
//
// A growable array class that keeps all elements in order using binary search

#ifndef VPC_TIER1_UTLSORTVECTOR_H_
#define VPC_TIER1_UTLSORTVECTOR_H_

#include "utlvector.h"

// This in an sorted order-preserving vector. Items may be inserted or removed
// at any point in the vector. When an item is inserted, all elements are
// moved down by one element using memmove. When an item is removed, all
// elements are shifted back down. Items are searched for in the vector
// using a binary search technique. Clients must pass in a Less() function
// into the constructor of the vector to determine the sort order.

#ifndef _WIN32
// gcc has no qsort_s, so i need to use a static var to hold the sort context.
// this makes cutlsortvector _not_ thread sfae under linux
extern void* g_pUtlSortVectorQSortContext;
#endif

template <class T>
class CUtlSortVectorDefaultLess {
 public:
  bool Less(const T& lhs, const T& rhs, void*) { return lhs < rhs; }
};

template <class T, class LessFunc = CUtlSortVectorDefaultLess<T>,
          class BaseVector = CUtlVector<T>>
class CUtlSortVector : public BaseVector {
 public:
  using BaseVector::Base;
  using BaseVector::Count;

  // constructor
  CUtlSortVector(intp nGrowSize = 0, intp initSize = 0);
  CUtlSortVector(T* pMemory, intp numElements);

  // inserts (copy constructs) an element in sorted order into the list
  intp Insert(const T& src);

  // Finds an element within the list using a binary search
  intp Find(const T& search) const;
  intp FindLessOrEqual(const T& search) const;
  intp FindLess(const T& search) const;
  template <typename K>
  intp FindAs(const K& key) const;

  // Removes a particular element
  void Remove(const T& search);
  void Remove(intp i);

  // Allows methods to set a context to be used with the less function..
  void SetLessContext(void* pCtx);

  // Note that you can only use this index until sorting is redone!!!
  intp InsertNoSort(const T& src);
  void RedoSort(bool bForceSort = false);

 protected:
  // No copy constructor
  CUtlSortVector(const CUtlSortVector<T, LessFunc>&);

  // never call these; illegal for this class
  int AddToHead();
  int AddToTail();
  int InsertBefore(int elem);
  int InsertAfter(int elem);
  int AddToHead(const T& src);
  int AddToTail(const T& src);
  int InsertBefore(int elem, const T& src);
  int InsertAfter(int elem, const T& src);
  int AddMultipleToHead(int num);
  int AddMultipleToTail(int num, const T* pToCopy = NULL);
  int InsertMultipleBefore(int elem, int num, const T* pToCopy = NULL);
  int InsertMultipleAfter(int elem, int num);
  int AddVectorToTail(CUtlVector<T> const& src);

  struct QSortContext_t {
    void* m_pLessContext;
    LessFunc* m_pLessFunc;
  };

#ifdef _WIN32
  static int CompareHelper(void* context, const T* lhs, const T* rhs) {
    QSortContext_t* ctx = reinterpret_cast<QSortContext_t*>(context);
    if (ctx->m_pLessFunc->Less(*lhs, *rhs, ctx->m_pLessContext)) return -1;
    if (ctx->m_pLessFunc->Less(*rhs, *lhs, ctx->m_pLessContext)) return 1;
    return 0;
  }
#else
  static int CompareHelper(const T* lhs, const T* rhs) {
    QSortContext_t* ctx =
        reinterpret_cast<QSortContext_t*>(g_pUtlSortVectorQSortContext);
    if (ctx->m_pLessFunc->Less(*lhs, *rhs, ctx->m_pLessContext)) return -1;
    if (ctx->m_pLessFunc->Less(*rhs, *lhs, ctx->m_pLessContext)) return 1;
    return 0;
  }
#endif

  void* m_pLessContext;
  bool m_bNeedsSort;

 private:
  void QuickSort(LessFunc& less, intp X, intp I);
};

//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
template <class T, class LessFunc, class BaseVector>
CUtlSortVector<T, LessFunc, BaseVector>::CUtlSortVector(intp nGrowSize,
                                                        intp initSize)
    : m_pLessContext(NULL),
      BaseVector(nGrowSize, initSize),
      m_bNeedsSort(false) {}

template <class T, class LessFunc, class BaseVector>
CUtlSortVector<T, LessFunc, BaseVector>::CUtlSortVector(T* pMemory,
                                                        intp numElements)
    : m_pLessContext(NULL),
      BaseVector(pMemory, numElements),
      m_bNeedsSort(false) {}

//-----------------------------------------------------------------------------
// Allows methods to set a context to be used with the less function..
//-----------------------------------------------------------------------------
template <class T, class LessFunc, class BaseVector>
void CUtlSortVector<T, LessFunc, BaseVector>::SetLessContext(void* pCtx) {
  m_pLessContext = pCtx;
}

//-----------------------------------------------------------------------------
// grows the vector
//-----------------------------------------------------------------------------
template <class T, class LessFunc, class BaseVector>
intp CUtlSortVector<T, LessFunc, BaseVector>::Insert(const T& src) {
  AssertFatal(!m_bNeedsSort);

  intp pos = FindLessOrEqual(src) + 1;
  this->GrowVector();
  this->ShiftElementsRight(pos);
  CopyConstruct<T>(&this->Element(pos), src);
  return pos;
}

template <class T, class LessFunc, class BaseVector>
intp CUtlSortVector<T, LessFunc, BaseVector>::InsertNoSort(const T& src) {
  m_bNeedsSort = true;
  intp lastElement = BaseVector::m_Size;
  // Just stick the new element at the end of the vector, but don't do a sort
  this->GrowVector();
  this->ShiftElementsRight(lastElement);
  CopyConstruct(&this->Element(lastElement), src);
  return lastElement;
}

template <class T, class LessFunc, class BaseVector>
void CUtlSortVector<T, LessFunc, BaseVector>::QuickSort(LessFunc& less,
                                                        intp nLower,
                                                        intp nUpper) {
#ifdef _WIN32
  typedef int(__cdecl * QSortCompareFunc_t)(void* context, const void*,
                                            const void*);
  if (this->Count() > 1) {
    QSortContext_t ctx;
    ctx.m_pLessContext = m_pLessContext;
    ctx.m_pLessFunc = &less;

    qsort_s(Base(), Count(), sizeof(T),
            (QSortCompareFunc_t)&CUtlSortVector<T, LessFunc>::CompareHelper,
            &ctx);
  }
#else
  typedef int(__cdecl * QSortCompareFunc_t)(const void*, const void*);
  if (this->Count() > 1) {
    QSortContext_t ctx;
    ctx.m_pLessContext = m_pLessContext;
    ctx.m_pLessFunc = &less;
    g_pUtlSortVectorQSortContext = &ctx;

    qsort(this->Base(), this->Count(), sizeof(T),
          (QSortCompareFunc_t)&CUtlSortVector<T, LessFunc>::CompareHelper);
  }
#endif
}

template <class T, class LessFunc, class BaseVector>
void CUtlSortVector<T, LessFunc, BaseVector>::RedoSort(
    bool bForceSort /*= false */) {
  if (!m_bNeedsSort && !bForceSort) return;

  m_bNeedsSort = false;
  LessFunc less;
  QuickSort(less, 0, this->Count() - 1);
}

//-----------------------------------------------------------------------------
// finds a particular element
//-----------------------------------------------------------------------------
template <class T, class LessFunc, class BaseVector>
intp CUtlSortVector<T, LessFunc, BaseVector>::Find(const T& src) const {
  AssertFatal(!m_bNeedsSort);

  LessFunc less;

  intp start = 0, end = this->Count() - 1;
  while (start <= end) {
    intp mid = (start + end) >> 1;
    if (less.Less(this->Element(mid), src, m_pLessContext)) {
      start = mid + 1;
    } else if (less.Less(src, this->Element(mid), m_pLessContext)) {
      end = mid - 1;
    } else {
      return mid;
    }
  }
  return -1;
}

//-----------------------------------------------------------------------------
// finds a particular element
//-----------------------------------------------------------------------------
template <class T, class LessFunc, class BaseVector>
template <typename K>
intp CUtlSortVector<T, LessFunc, BaseVector>::FindAs(const K& key) const {
  AssertFatal(!m_bNeedsSort);

  LessFunc less;

  intp start = 0, end = this->Count() - 1;
  while (start <= end) {
    intp mid = (start + end) >> 1;
    intp nResult = less.Compare(this->Element(mid), key, m_pLessContext);
    if (nResult < 0) {
      start = mid + 1;
    } else if (nResult > 0) {
      end = mid - 1;
    } else {
      return mid;
    }
  }
  return -1;
}

//-----------------------------------------------------------------------------
// finds a particular element
//-----------------------------------------------------------------------------
template <class T, class LessFunc, class BaseVector>
intp CUtlSortVector<T, LessFunc, BaseVector>::FindLessOrEqual(
    const T& src) const {
  AssertFatal(!m_bNeedsSort);

  LessFunc less;
  intp start = 0, end = this->Count() - 1;
  while (start <= end) {
    intp mid = (start + end) >> 1;
    if (less.Less(this->Element(mid), src, m_pLessContext)) {
      start = mid + 1;
    } else if (less.Less(src, this->Element(mid), m_pLessContext)) {
      end = mid - 1;
    } else {
      return mid;
    }
  }
  return end;
}

template <class T, class LessFunc, class BaseVector>
intp CUtlSortVector<T, LessFunc, BaseVector>::FindLess(const T& src) const {
  AssertFatal(!m_bNeedsSort);

  LessFunc less;
  intp start = 0, end = this->Count() - 1;
  while (start <= end) {
    intp mid = (start + end) >> 1;
    if (less.Less(this->Element(mid), src, m_pLessContext)) {
      start = mid + 1;
    } else {
      end = mid - 1;
    }
  }
  return end;
}

//-----------------------------------------------------------------------------
// Removes a particular element
//-----------------------------------------------------------------------------
template <class T, class LessFunc, class BaseVector>
void CUtlSortVector<T, LessFunc, BaseVector>::Remove(const T& search) {
  AssertFatal(!m_bNeedsSort);

  intp pos = Find(search);
  if (pos != -1) {
    BaseVector::Remove(pos);
  }
}

template <class T, class LessFunc, class BaseVector>
void CUtlSortVector<T, LessFunc, BaseVector>::Remove(intp i) {
  BaseVector::Remove(i);
}

#endif  // VPC_TIER1_UTLSORTVECTOR_H_
