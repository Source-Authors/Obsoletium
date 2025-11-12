//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#ifndef UTIL_REGISTRY_H
#define UTIL_REGISTRY_H

#include <cstdint>
#include <memory>

#include "tier0/commonmacros.h"


//-----------------------------------------------------------------------------
// Purpose: Interface to registry
//-----------------------------------------------------------------------------
abstract_class IRegistry
{
public:
	// We have to have a virtual destructor since otherwise the derived class destructors
	// will not be called.
	virtual ~IRegistry() {}

	// Init/shutdown
	virtual bool			Init( const char *platformName ) = 0;
	virtual void			Shutdown( void ) = 0;

	// Read/write integers
	virtual int				ReadInt( const char *key, int defaultValue = 0 ) = 0;
	virtual void			WriteInt( const char *key, int value ) = 0;

	// Read/write strings
	virtual const char		*ReadString( const char *key, const char *defaultValue = 0 ) = 0;
	virtual void			WriteString( const char *key, const char *value ) = 0;

	// Read/write helper methods
	virtual int				ReadInt( const char *pKeyBase, const char *pKey, int defaultValue = 0 ) = 0;
	virtual void			WriteInt( const char *pKeyBase, const char *key, int value ) = 0;
	virtual const char		*ReadString( const char *pKeyBase, const char *key, const char *defaultValue ) = 0;
	virtual void			WriteString( const char *pKeyBase, const char *key, const char *value ) = 0;

	// dimhotepus: Extended APIs below.
	
	// Read/write 64 bit integers
	virtual int64_t			ReadInt64( const char *key, int64_t defaultValue = 0 ) = 0;
	virtual void			WriteInt64( const char *key, int64_t value ) = 0;
};

extern IRegistry *registry;

struct RegistryDeleter
{
	void operator()(IRegistry *r) { r->Shutdown(); }
};

// Creates it and calls Init
std::unique_ptr<IRegistry, RegistryDeleter> InstanceRegistry( char const *subDirectoryUnderValve );

#endif  // UTIL_REGISTRY_H
