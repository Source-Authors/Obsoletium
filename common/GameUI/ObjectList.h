//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// ObjectList.h: interface for the ObjectList class.
//
//////////////////////////////////////////////////////////////////////

#ifndef OBJECTLIST_H
#define OBJECTLIST_H

#pragma once

#include "IObjectContainer.h"	// Added by ClassView

class ObjectList : public IObjectContainer  
{
public:
	void	Init() override;
	bool	Add( void * newObject ) override;
	void *	GetFirst() override;
	void *	GetNext() override;
	


	ObjectList();
	~ObjectList() override;

	void	Clear( bool freeElementsMemory ) override;
	int		CountElements() override;
	void *	RemoveTail();
	void *	RemoveHead();

	bool	AddTail(void * newObject);
	bool	AddHead(void * newObject);
	bool	Remove(void * object) override;
	bool	Contains(void * object) override;
	bool	IsEmpty() override;

	using element_t = struct element_s {
		element_s *	prev;	// pointer to the last element or NULL
		element_s *	next;	// pointer to the next elemnet or NULL
		void *		object;	// the element's object
	};

protected:

	element_t *	head;	// first element in list
	element_t *	tail;	// last element in list
	element_t *	current; // current element in list
	int			number;

};

#endif // !defined
