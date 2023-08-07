//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "tier1/interface.h"
#include "vphysics/object_hash.h"
#include "vphysics/collision_set.h"
#include "tier1/tier1.h"
#include "ivu_vhash.hxx"

#include "vphysics_interfaceV30.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// simple 32x32 bit array
class CPhysicsCollisionSet : public IPhysicsCollisionSet
{
public:
	~CPhysicsCollisionSet() {}
	CPhysicsCollisionSet()
	{
		memset( m_bits, 0, sizeof(m_bits) );
	}
	void EnableCollisions( int index0, int index1 )
	{
		Assert(index0<32&&index1<32);
		m_bits[index0] |= 1<<index1;
		m_bits[index1] |= 1<<index0;
	}
	void DisableCollisions( int index0, int index1 )
	{
		Assert(index0<32&&index1<32);
		m_bits[index0] &= ~(1<<index1);
		m_bits[index1] &= ~(1<<index0);
	}

	bool ShouldCollide( int index0, int index1 )
	{
		Assert(index0<32&&index1<32);
		return (m_bits[index0] & (1<<index1)) ? true : false;
	}
private:
	unsigned int m_bits[32];
};


//-----------------------------------------------------------------------------
// Main physics interface
//-----------------------------------------------------------------------------
class CPhysicsInterface : public CTier1AppSystem<IPhysics>
{
public:
	CPhysicsInterface() : m_pCollisionSetHash(NULL) {}
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual	IPhysicsEnvironment *CreateEnvironment( void );
	virtual void DestroyEnvironment( IPhysicsEnvironment *pEnvironment );
	virtual IPhysicsEnvironment *GetActiveEnvironmentByIndex( int index );
	virtual IPhysicsObjectPairHash *CreateObjectPairHash();
	virtual void DestroyObjectPairHash( IPhysicsObjectPairHash *pHash );
	virtual IPhysicsCollisionSet *FindOrCreateCollisionSet( unsigned int id, int maxElementCount );
	virtual IPhysicsCollisionSet *FindCollisionSet( unsigned int id );
	virtual void DestroyAllCollisionSets();

private:
	CUtlVector<IPhysicsEnvironment *>	m_envList;
	CUtlVector<CPhysicsCollisionSet>	m_collisionSets;
	IVP_VHash_Store						*m_pCollisionSetHash;
};


//-----------------------------------------------------------------------------
// Expose singleton
//-----------------------------------------------------------------------------
static CPhysicsInterface g_MainDLLInterface;
IPhysics *g_PhysicsInternal = &g_MainDLLInterface;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPhysicsInterface, IPhysics, VPHYSICS_INTERFACE_VERSION, g_MainDLLInterface );


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CPhysicsInterface::QueryInterface( const char *pInterfaceName )
{
	// Loading the datacache DLL mounts *all* interfaces
	// This includes the backward-compatible interfaces + other vphysics interfaces
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, NULL );				// to prevent the LTCG compiler from crashing.
}


//-----------------------------------------------------------------------------
// Implementation of IPhysics
//-----------------------------------------------------------------------------
IPhysicsEnvironment *CPhysicsInterface::CreateEnvironment( void )
{
	IPhysicsEnvironment *pEnvironment = CreatePhysicsEnvironment();
	m_envList.AddToTail( pEnvironment );
	return pEnvironment;
}

void CPhysicsInterface::DestroyEnvironment( IPhysicsEnvironment *pEnvironment )
{
	m_envList.FindAndRemove( pEnvironment );
	delete pEnvironment;
}

IPhysicsEnvironment	*CPhysicsInterface::GetActiveEnvironmentByIndex( int index )
{
	if ( index < 0 || index >= m_envList.Count() )
		return NULL;

	return m_envList[index];
}

IPhysicsObjectPairHash *CPhysicsInterface::CreateObjectPairHash()
{
	return ::CreateObjectPairHash();
}

void CPhysicsInterface::DestroyObjectPairHash( IPhysicsObjectPairHash *pHash )
{
	delete pHash;
}
// holds a cache of these by id.
// NOTE: This is stuffed into vphysics.dll as a sneaky way of sharing the memory between
// client and server in single player.  So you can't have different client/server rules.
IPhysicsCollisionSet *CPhysicsInterface::FindOrCreateCollisionSet( unsigned int id, int maxElementCount )
{
	if ( !m_pCollisionSetHash )
	{
		m_pCollisionSetHash = new IVP_VHash_Store(256);
	}
	Assert( id != 0 );
	Assert( maxElementCount <= 32 );
	if ( maxElementCount > 32 )
		return NULL;

	IPhysicsCollisionSet *pSet = FindCollisionSet( id );
	if ( pSet )
		return pSet;
	intp index = m_collisionSets.AddToTail();
	m_pCollisionSetHash->add_elem( (void *)(intp)id, (void *)(intp)(index+1) );
	return &m_collisionSets[index];
}

IPhysicsCollisionSet *CPhysicsInterface::FindCollisionSet( unsigned int id )
{
	if ( m_pCollisionSetHash )
	{
		intp index = (intp)m_pCollisionSetHash->find_elem( (void *)(intp)id );
		if ( index > 0 )
		{
			Assert( index <= m_collisionSets.Count() );
			if ( index <= m_collisionSets.Count() )
			{
				return &m_collisionSets[index-1];
			}
		}
	}
	return NULL;
}

void CPhysicsInterface::DestroyAllCollisionSets()
{
	m_collisionSets.Purge();
	delete m_pCollisionSetHash;
	m_pCollisionSetHash = NULL;
}
