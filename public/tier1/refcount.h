//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Tools for correctly implementing & handling reference counted objects.

#ifndef REFCOUNT_H
#define REFCOUNT_H

#include "tier0/threadtools.h"

#include <atomic>

template <typename T>
inline void SafeAssign(T** ppInoutDst, T* pInoutSrc )
{
	Assert( ppInoutDst );

	// Do addref before release
	if ( pInoutSrc )
		( pInoutSrc )->AddRef();

	// Do addref before release
	if ( *ppInoutDst )
		( *ppInoutDst )->Release();

	// Do the assignment
	( *ppInoutDst ) = pInoutSrc;
}

template <typename T>
inline void SafeAddRef( T* pObj )
{
	if ( pObj )
		pObj->AddRef();
}

template <typename T>
inline void SafeRelease( T** ppInoutPtr )
{
	Assert( ppInoutPtr  );
	if ( *ppInoutPtr )
		( *ppInoutPtr )->Release();

	*ppInoutPtr = nullptr;
}

// Implement a standard reference counted interface.  Use of this is optional
// insofar as all the concrete tools only require at compile time that the
// function signatures match.
// dimhotepus: Add NO_VTABLE.
struct NO_VTABLE IRefCounted
{
	virtual int AddRef() = 0;
	virtual int Release() = 0;
};


// Release a pointer and mark it nullptr.
template <class REFCOUNTED_ITEM_PTR>
inline int SafeRelease( REFCOUNTED_ITEM_PTR &pRef )
{
	// Use funny syntax so that this works on "auto pointers".
	auto *ppRef = &pRef;
	if ( *ppRef )
	{
		int result = (*ppRef)->Release();
		*ppRef = nullptr;
		return result;
	}
	return 0;
}

// Maintain a reference across a scope.
template <class T = IRefCounted>
class CAutoRef
{
public:
	// dimhotepus: Mark explicit.
	explicit CAutoRef( T *pRef ) : m_pRef( pRef )
	{
		if ( m_pRef )
			m_pRef->AddRef();
	}

   ~CAutoRef()
   {
      if (m_pRef)
         m_pRef->Release();
   }

private:
   T *m_pRef;
};

// Do a an inline AddRef then return the pointer, useful when returning an
// object from a function.
template<typename T>
inline T* RetAddRef( T *p ) { p->AddRef(); return p; }

template<typename T>
inline T* InlineAddRef( T *p ) { p->AddRef(); return p; } //-V524


// A class to both hold a pointer to an object and its reference.  Base exists
// to support other cleanup models.
template <class T>
class CBaseAutoPtr
{
public:
	CBaseAutoPtr()	: CBaseAutoPtr(nullptr) {}
	// dimhotepus: Mark explicit.
	explicit CBaseAutoPtr(T *pFrom) : m_pObject(pFrom) {}

	[[nodiscard]] operator const void *() const							{ return m_pObject; }
	[[nodiscard]] operator void *()										{ return m_pObject; }

	[[nodiscard]] operator const T *() const							{ return m_pObject; }
	[[nodiscard]] operator const T *()									{ return m_pObject; }
	[[nodiscard]] operator T *()										{ return m_pObject; }

	// dimhotepus: Drop unsafe type casts.
	//[[nodiscard]] int		operator=( int i )							{ AssertMsg( i == 0, "Only NULL allowed on integer assign" ); m_pObject = 0; return 0; }
	[[nodiscard]] T *		operator=( T *p )							{ m_pObject = p; return p; }

	[[nodiscard]] bool		operator !() const							{ return ( !m_pObject ); }
	// dimhotepus: Drop unsafe type casts.
	// [[nodiscard]] bool		operator!=( int i ) const					{ AssertMsg( i == 0, "Only NULL allowed on integer compare" ); return (m_pObject != NULL); }
	[[nodiscard]] bool		operator==( const void *p ) const			{ return ( m_pObject == p ); }
	[[nodiscard]] bool		operator!=( const void *p ) const			{ return ( m_pObject != p ); }
	[[nodiscard]] bool		operator==( T *p ) const					{ return operator==( static_cast<void *>(p) ); }
	[[nodiscard]] bool		operator!=( T *p ) const					{ return operator!=( static_cast<void *>(p) ); }
	[[nodiscard]] bool		operator==( const CBaseAutoPtr<T> &p ) const { return operator==( static_cast<const void *>(p) ); }
	[[nodiscard]] bool		operator!=( const CBaseAutoPtr<T> &p ) const { return operator!=( static_cast<const void *>(p) ); }

	[[nodiscard]] T *  		operator->()								{ return m_pObject; }
	[[nodiscard]] T &  		operator *()								{ return *m_pObject; }
	[[nodiscard]] T ** 		operator &()								{ return &m_pObject; }

	[[nodiscard]] const T *   operator->() const							{ return m_pObject; }
	[[nodiscard]] const T &   operator *() const							{ return *m_pObject; }
	[[nodiscard]] T * const * operator &() const							{ return &m_pObject; }

protected:
	CBaseAutoPtr( const CBaseAutoPtr<T> &from ) : m_pObject( from.m_pObject ) {}
	CBaseAutoPtr<T>& operator=( const CBaseAutoPtr<T> &from ) { m_pObject = from.m_pObject; return *this; }

	T *m_pObject;
};

template <class T>
class CRefPtr : public CBaseAutoPtr<T>
{
	using BaseClass = CBaseAutoPtr<T>;

public:
	CRefPtr()	= default;
	// dimhotepus: Mark explicit.
	explicit CRefPtr( T *pInit )							: BaseClass( pInit ) {}
	CRefPtr( const CRefPtr<T> &from )						: BaseClass( from ) {}
	~CRefPtr()												{ if ( BaseClass::m_pObject ) BaseClass::m_pObject->Release(); }

	CRefPtr<T>& operator=( const CRefPtr<T> &from )			{ BaseClass::operator=( from ); return *this; }

	// dimhotepus: Drop unsafe type casts.
	//int operator=( int i )									{ return BaseClass::operator=( i ); }
	T *operator=( T *p )									{ return BaseClass::operator=( p ); }

	[[nodiscard]] operator bool() const						{ return !BaseClass::operator!(); }
	[[nodiscard]] operator bool()							{ return !BaseClass::operator!(); }

	void SafeRelease()										{ if ( BaseClass::m_pObject ) BaseClass::m_pObject->Release(); BaseClass::m_pObject = nullptr; }
	void AssignAddRef( T *pFrom )							{ SafeRelease(); if (pFrom) pFrom->AddRef(); BaseClass::m_pObject = pFrom; }
	void AddRefAssignTo( T *&pTo )							{ ::SafeRelease( pTo ); if ( BaseClass::m_pObject ) BaseClass::m_pObject->AddRef(); pTo = BaseClass::m_pObject; }
};


// Traits class defining multithreaded reference count threading model.
class CRefMT
{
public:
	// dimhotepus: Mark explicit.
	explicit CRefMT(int initial) : m_Ref{initial} {}

	int Increment() { return m_Ref.fetch_add(1, std::memory_order::memory_order_relaxed) + 1; }
	// Acquire-release because destructor in some smart pointer impl depends on it.
	int Decrement() { return m_Ref.fetch_sub(1, std::memory_order::memory_order_acq_rel) - 1; }
	
	// Acquire-release because destructor in some smart pointer impl depends on it.
	[[nodiscard]] int Counter() const { return m_Ref.load(std::memory_order_acq_rel); }

private:
	std::atomic_int m_Ref;
};

// Traits class defining singlethreaded reference count threading model.
class CRefST
{
public:
	// dimhotepus: Mark explicit.
	explicit CRefST(int initial) : m_Ref{initial} {}

	int Increment() { return ++m_Ref; }
	int Decrement() { return --m_Ref; }

	[[nodiscard]] int Counter() const { return m_Ref; }

private:
	int m_Ref;
};

// Actual reference counting implementation.  Pulled out to reduce code bloat.
template <const bool bSelfDelete, typename CRefThreading = CRefMT>
class NO_VTABLE CRefCountServiceBase
{
protected:
	CRefCountServiceBase() : m_iRefs( 1 )
	{
	}

	virtual ~CRefCountServiceBase() = default;

	[[nodiscard]] virtual bool OnFinalRelease()
	{
		return true;
	}

	[[nodiscard]] int GetRefCount() const
	{
		return m_iRefs.Counter();
	}

	int DoAddRef()
	{
		return m_iRefs.Increment();
	}

	int DoRelease()
	{
		const int result = m_iRefs.Decrement();
		if ( result )
			return result;
		if ( OnFinalRelease() && bSelfDelete )
			delete this;
		return 0;
	}

private:
	CRefThreading m_iRefs;
};

class CRefCountServiceNull
{
protected:
	static int DoAddRef() { return 1; }
	static int DoRelease() { return 1; }
};

template <typename CRefThreading = CRefMT>
class NO_VTABLE CRefCountServiceDestruct
{
protected:
	CRefCountServiceDestruct() : m_iRefs( 1 )
	{
	}

	virtual ~CRefCountServiceDestruct() = default;

	[[nodiscard]] int GetRefCount() const
	{
		return m_iRefs.Counter();
	}

	int DoAddRef()
	{
		return m_iRefs.Increment();
	}

	int DoRelease()
	{
		const int result = m_iRefs.Decrement();
		if ( result )
			return result;
		this->~CRefCountServiceDestruct();
		return 0;
	}

private:
	CRefThreading m_iRefs;
};


using CRefCountServiceST = CRefCountServiceBase<true, CRefST>;
using CRefCountServiceNoDeleteST = CRefCountServiceBase<false, CRefST>;

using CRefCountServiceMT = CRefCountServiceBase<true, CRefMT>;
using CRefCountServiceNoDeleteMT = CRefCountServiceBase<false, CRefMT>;

// Default to threadsafe
using CRefCountServiceNoDelete = CRefCountServiceNoDeleteMT;
using CRefCountService = CRefCountServiceMT;

//-----------------------------------------------------------------------------
// Purpose:	Base classes to implement reference counting
//-----------------------------------------------------------------------------

template < class REFCOUNT_SERVICE = CRefCountService > 
class NO_VTABLE CRefCounted : public REFCOUNT_SERVICE
{
public:
  virtual ~CRefCounted() = default;
	int AddRef() 			{ return REFCOUNT_SERVICE::DoAddRef(); }
	int Release()			{ return REFCOUNT_SERVICE::DoRelease(); }
};

//-------------------------------------

template < class BASE1, class REFCOUNT_SERVICE = CRefCountService > 
class NO_VTABLE CRefCounted1 : public BASE1,
							   public REFCOUNT_SERVICE
{
public:
	virtual ~CRefCounted1() = default;
	int AddRef() 			{ return REFCOUNT_SERVICE::DoAddRef(); }
	int Release()			{ return REFCOUNT_SERVICE::DoRelease(); }
};

//-------------------------------------

template < class BASE1, class BASE2, class REFCOUNT_SERVICE = CRefCountService > 
class NO_VTABLE CRefCounted2 : public BASE1, public BASE2,
							   public REFCOUNT_SERVICE
{
public:
  virtual ~CRefCounted2() = default;
	int AddRef() 			{ return REFCOUNT_SERVICE::DoAddRef(); }
	int Release()			{ return REFCOUNT_SERVICE::DoRelease(); }
};

//-------------------------------------

template < class BASE1, class BASE2, class BASE3, class REFCOUNT_SERVICE = CRefCountService > 
class NO_VTABLE CRefCounted3 : public BASE1, public BASE2, public BASE3,
							   public REFCOUNT_SERVICE
{
  virtual ~CRefCounted3() = default;
	int AddRef() 			{ return REFCOUNT_SERVICE::DoAddRef(); }
	int Release()			{ return REFCOUNT_SERVICE::DoRelease(); }
};

//-------------------------------------

template < class BASE1, class BASE2, class BASE3, class BASE4, class REFCOUNT_SERVICE = CRefCountService > 
class NO_VTABLE CRefCounted4 : public BASE1, public BASE2, public BASE3, public BASE4,
							   public REFCOUNT_SERVICE
{
public:
  virtual ~CRefCounted4() = default;
	int AddRef() 			{ return REFCOUNT_SERVICE::DoAddRef(); }
	int Release()			{ return REFCOUNT_SERVICE::DoRelease(); }
};

//-------------------------------------

template < class BASE1, class BASE2, class BASE3, class BASE4, class BASE5, class REFCOUNT_SERVICE = CRefCountService > 
class NO_VTABLE CRefCounted5 : public BASE1, public BASE2, public BASE3, public BASE4, public BASE5,
							   public REFCOUNT_SERVICE
{
public:
  virtual ~CRefCounted5() = default;
	int AddRef() 			{ return REFCOUNT_SERVICE::DoAddRef(); }
	int Release()			{ return REFCOUNT_SERVICE::DoRelease(); }
};

//-----------------------------------------------------------------------------
// Purpose:	Class to throw around a reference counted item to debug
//			referencing problems
//-----------------------------------------------------------------------------

template <class BASE_REFCOUNTED, int FINAL_REFS, const char *pszName>
class CRefDebug : public BASE_REFCOUNTED
{
public:
#ifdef _DEBUG
	CRefDebug()
	{
		AssertMsg( this->GetRefCount() == 1, "Expected initial ref count of 1" );
		DevMsg( "%s:create 0x%x\n", ( pszName ) ? pszName : "", this );
	}

	virtual ~CRefDebug()
	{
		AssertDevMsg( this->GetRefCount() == FINAL_REFS, "Object still referenced on destroy?" );
		DevMsg( "%s:destroy 0x%x\n", ( pszName ) ? pszName : "", this );
	}

	int AddRef()
	{
		DevMsg( "%s:(0x%x)->AddRef() --> %d\n", ( pszName ) ? pszName : "", this, this->GetRefCount() + 1 );
		return BASE_REFCOUNTED::AddRef();
	}

	int Release()
	{
		DevMsg( "%s:(0x%x)->Release() --> %d\n", ( pszName ) ? pszName : "", this, this->GetRefCount() - 1 );
		Assert( this->GetRefCount() > 0 );
		return BASE_REFCOUNTED::Release();
	}
#endif
};

//-----------------------------------------------------------------------------

#endif // REFCOUNT_H
