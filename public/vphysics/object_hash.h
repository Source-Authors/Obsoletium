//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef OBJECT_HASH_H
#define OBJECT_HASH_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"

abstract_class IPhysicsObjectPairHash
{
public:
	virtual ~IPhysicsObjectPairHash() {}
	virtual void AddObjectPair( void *pObject0, void *pObject1 ) = 0;
	virtual void RemoveObjectPair( void *pObject0, void *pObject1 ) = 0;
	[[nodiscard]] virtual bool IsObjectPairInHash( void *pObject0, void *pObject1 ) = 0;
	virtual void RemoveAllPairsForObject( void *pObject0 ) = 0;
	[[nodiscard]] virtual bool IsObjectInHash( void *pObject0 ) = 0;

	// Used to iterate over all pairs an object is part of
	[[nodiscard]] virtual int GetPairCountForObject( void *pObject0 ) = 0;
	[[nodiscard]] virtual int GetPairListForObject( void *pObject0, int nMaxCount, void **ppObjectList ) = 0;
};


#endif // OBJECT_HASH_H
