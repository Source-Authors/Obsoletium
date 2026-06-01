#ifndef UTLMAPMT_H
#define UTLMAPMT_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/dbg.h"
#include "utlmap.h"

//-----------------------------------------------------------------------------
//
// Purpose:	Same as CUtlMap but threadsafe
//
//-----------------------------------------------------------------------------

template <typename K, typename T, typename I = unsigned short>
class CUtlMapMT : public CUtlMap<K, T, I>
{
public:
	using Base = CUtlMap<K, T, I>;
	using KeyType_t = typename Base::KeyType_t;
	using ElemType_t = typename Base::ElemType_t;
	using IndexType_t = typename Base::IndexType_t;
	using LessFunc_t = typename Base::LessFunc_t;

	// Constructors
	CUtlMapMT(intp growSize = 0, intp initSize = 0, LessFunc_t lessfunc = 0)
		: Base(growSize, initSize, lessfunc) {}
	
	CUtlMapMT(LessFunc_t lessfunc) : Base(lessfunc) {}

	void EnsureCapacity(intp num) { AUTO_LOCK_WRITE(m_lock); Base::EnsureCapacity(num); }

	ElemType_t &Element(IndexType_t i) { AUTO_LOCK_READ(m_lock); return Base::Element(i); }
	const ElemType_t &Element(IndexType_t i) const { AUTO_LOCK_READ(m_lock); return Base::Element(i); }

	ElemType_t &operator[](IndexType_t i) { AUTO_LOCK_READ(m_lock); return Base::operator[](i); }
	const ElemType_t &operator[](IndexType_t i) const { AUTO_LOCK_READ(m_lock); return Base::operator[](i); }

	KeyType_t &Key(IndexType_t i) { AUTO_LOCK_READ(m_lock); return Base::Key(i); }
	const KeyType_t &Key(IndexType_t i) const { AUTO_LOCK_READ(m_lock); return Base::Key(i); }

	size_t Count() const { AUTO_LOCK_READ(m_lock); return Base::Count(); }
	IndexType_t MaxElement() const { AUTO_LOCK_READ(m_lock); return Base::MaxElement(); }
	bool IsValidIndex(IndexType_t i) const { AUTO_LOCK_READ(m_lock); return Base::IsValidIndex(i); }
	bool IsValid() const { AUTO_LOCK_READ(m_lock); return Base::IsValid(); }

	void SetLessFunc(LessFunc_t func) { AUTO_LOCK_WRITE(m_lock); Base::SetLessFunc(func); }

	IndexType_t Insert(const KeyType_t &key, const ElemType_t &insert) { AUTO_LOCK_WRITE(m_lock); return Base::Insert(key, insert); }
	IndexType_t Insert(const KeyType_t &key) { AUTO_LOCK_WRITE(m_lock); return Base::Insert(key); }

	IndexType_t Find(const KeyType_t &key) const { AUTO_LOCK_READ(m_lock); return Base::Find(key); }

	void RemoveAt(IndexType_t i) { AUTO_LOCK_WRITE(m_lock); Base::RemoveAt(i); }
	bool Remove(const KeyType_t &key) { AUTO_LOCK_WRITE(m_lock); return Base::Remove(key); }
	void RemoveAll() { AUTO_LOCK_WRITE(m_lock); Base::RemoveAll(); }
	void Purge() { AUTO_LOCK_WRITE(m_lock); Base::Purge(); }
	void PurgeAndDeleteElements() { AUTO_LOCK_WRITE(m_lock); Base::PurgeAndDeleteElements(); }

	IndexType_t FirstInorder() const { AUTO_LOCK_READ(m_lock); return Base::FirstInorder(); }
	IndexType_t NextInorder(IndexType_t i) const { AUTO_LOCK_READ(m_lock); return Base::NextInorder(i); }
	IndexType_t PrevInorder(IndexType_t i) const { AUTO_LOCK_READ(m_lock); return Base::PrevInorder(i); }
	IndexType_t LastInorder() const { AUTO_LOCK_READ(m_lock); return Base::LastInorder(); }

	void Reinsert(const KeyType_t &key, IndexType_t i) { AUTO_LOCK_WRITE(m_lock); Base::Reinsert(key, i); }

	IndexType_t InsertOrReplace(const KeyType_t &key, const ElemType_t &insert)
	{
		AUTO_LOCK_WRITE(m_lock);
		return Base::InsertOrReplace(key, insert);
	}

	void Swap(CUtlMap<K, T, I> &that)
	{
		// Lock both maps to avoid race
		AUTO_LOCK_WRITE(m_lock);
		AUTO_LOCK_WRITE(static_cast<CUtlMapMT<K,T,I>&>(that).m_lock);
		Base::Swap(that);
	}

private:
#if defined(WIN32) || defined(_WIN32)
	mutable CThreadSpinRWLock m_lock;
#else
	mutable CThreadRWLock m_lock;
#endif
};

#endif // UTLMAPMT_H
