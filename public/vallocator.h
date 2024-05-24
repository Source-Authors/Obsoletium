//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

// These classes let you write your own allocators to be used with new and delete.
// If you have an allocator: VAllocator *pAlloc, you can call new and delete like this:
//
// ptr = VNew(pAlloc) ClassName;
// VDelete(pAlloc, ptr);
//
// Note: allocating and freeing arrays of objects will not work using VAllocators.



#ifndef VALLOCATOR_H
#define VALLOCATOR_H

#include <cstddef>

class VAllocator
{
public:
	virtual void*	Alloc(size_t size) = 0;
	virtual void	Free(void *ptr) = 0;
};


// This allocator just uses malloc and free.
class VStdAllocator : public VAllocator
{
public:
	void*	Alloc(size_t size) override;
	void	Free(void *ptr) override;
};

extern VStdAllocator g_StdAllocator;


// Use these to allocate classes through VAllocator.
// Allocating arrays of classes is not supported.
#define VNew(pAlloc)				new 
#define VDelete(pAlloc, ptr)		delete ptr

// Used internally.. just makes sure we call the right operator new.
class DummyAllocatorHelper
{
public:
	int x;
};

inline void* operator new(size_t size, void *ptr, DummyAllocatorHelper *asdf)
{
	(void)asdf;	// Suppress unused-variable compiler warnings.
	(void)size;
	return ptr;
}

inline void operator delete(void *ptrToDelete, void *ptr, DummyAllocatorHelper *asdf)
{
	(void)asdf;	// Suppress unused-variable compiler warnings.
	(void)ptr;
	(void)ptrToDelete;
}

// Use these to manually construct and destruct lists of objects.
template<class T>
inline void VAllocator_CallConstructors(T *pObjects, ptrdiff_t count=1)
{
	for(ptrdiff_t i=0; i < count; i++)
		new(&pObjects[i], (DummyAllocatorHelper*)0) T;
}

template<class T>
inline void VAllocator_CallDestructors(T *pObjects, ptrdiff_t count)
{
	for(ptrdiff_t i=0; i < count; i++)
		pObjects[i].~T();
}

#endif

