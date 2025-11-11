//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "iregistry.h"

#include <cstdio>

#include "tier0/platform.h"
#include "tier0/vcrmode.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"

#if defined(WIN32)
#include "winlite.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Exposes registry interface to rest of launcher
//-----------------------------------------------------------------------------
class CRegistry : public IRegistry
{
public:
	CRegistry();
	virtual	~CRegistry();

	bool			Init( const char *platformName ) override;
	bool			DirectInit( const char *subDirectoryUnderValve );
	void			Shutdown() override;
	
	int				ReadInt( const char *key, int defaultValue = 0) override;
	void			WriteInt( const char *key, int value ) override;

	const char		*ReadString( const char *key, const char *defaultValue = nullptr ) override;
	void			WriteString( const char *key, const char *value ) override;

	// Read/write helper methods
	int				ReadInt( const char *pKeyBase, const char *pKey, int defaultValue = 0 ) override;
	void			WriteInt( const char *pKeyBase, const char *key, int value ) override;
	const char		*ReadString( const char *pKeyBase, const char *key, const char *defaultValue ) override;
	void			WriteString( const char *pKeyBase, const char *key, const char *value ) override;

	// dimhotepus:

	// Read/write 64 bit integers
	int64			ReadInt64( const char *key, int64 defaultValue = 0 ) override;
	void			WriteInt64( const char *key, int64 value ) override;

private:
	bool			m_bValid;
#ifdef WIN32
	HKEY			m_hKey;
#endif
};

// Creates it and calls Init
std::unique_ptr<IRegistry, RegistryDeleter> InstanceRegistry( char const *subDirectoryUnderValve )
{
	auto *instance = new CRegistry();
	instance->DirectInit( subDirectoryUnderValve );
	return std::unique_ptr<IRegistry, RegistryDeleter>( instance );
}

// Expose to launcher
static CRegistry g_Registry;
IRegistry *registry = &g_Registry;

//-----------------------------------------------------------------------------
// Read/write helper methods
//-----------------------------------------------------------------------------
int CRegistry::ReadInt( const char *pKeyBase, const char *pKey, int defaultValue )
{
	intp nLen = V_strlen( pKeyBase );
	intp nKeyLen = V_strlen( pKey );
	char *pFullKey = stackallocT( char, nLen + nKeyLen + 2 );
	Q_snprintf( pFullKey, nLen + nKeyLen + 2, "%s\\%s", pKeyBase, pKey );
	return ReadInt( pFullKey, defaultValue );
}

void CRegistry::WriteInt( const char *pKeyBase, const char *pKey, int value )
{
	intp nLen = V_strlen( pKeyBase );
	intp nKeyLen = V_strlen( pKey );
	char *pFullKey = stackallocT( char, nLen + nKeyLen + 2 );
	Q_snprintf( pFullKey, nLen + nKeyLen + 2, "%s\\%s", pKeyBase, pKey );
	WriteInt( pFullKey, value );
}

const char *CRegistry::ReadString( const char *pKeyBase, const char *pKey, const char *defaultValue )
{
	intp nLen = V_strlen( pKeyBase );
	intp nKeyLen = V_strlen( pKey );
	char *pFullKey = stackallocT( char, nLen + nKeyLen + 2 );
	Q_snprintf( pFullKey, nLen + nKeyLen + 2, "%s\\%s", pKeyBase, pKey );
	return ReadString( pFullKey, defaultValue );
}

void CRegistry::WriteString( const char *pKeyBase, const char *pKey, const char *value )
{
	intp nLen = V_strlen( pKeyBase );
	intp nKeyLen = V_strlen( pKey );
	char *pFullKey = stackallocT( char, nLen + nKeyLen + 2 );
	Q_snprintf( pFullKey, nLen + nKeyLen + 2, "%s\\%s", pKeyBase, pKey );
	WriteString( pFullKey, value );
}

#ifndef POSIX

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRegistry::CRegistry()
{
	// Assume failure
	m_bValid	= false;
	m_hKey		= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRegistry::~CRegistry()
{
}

//-----------------------------------------------------------------------------
// Purpose: Read integer from registry
// Input  : *key - 
//			defaultValue - 
// Output : int
//-----------------------------------------------------------------------------
int CRegistry::ReadInt( const char *key, int defaultValue /*= 0*/ )
{
	DWORD dwType;           // Type of key

	int value;

	if ( !m_bValid )
	{
		return defaultValue;
	}

	DWORD dwSize = sizeof( value );

	LONG lResult = VCRHook_RegQueryValueEx(
		m_hKey,		// handle to key
		key,	// value name
		0,			// reserved
		&dwType,    // type buffer
		(LPBYTE)&value,    // data buffer
		&dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return defaultValue;

	if (dwType != REG_DWORD)
		return defaultValue;

	return value;
}

//-----------------------------------------------------------------------------
// Purpose: Save integer to registry
// Input  : *key - 
//			value - 
//-----------------------------------------------------------------------------
void CRegistry::WriteInt( const char *key, int value )
{
	if ( !m_bValid )
	{
		return;
	}

	constexpr DWORD dwSize = sizeof( value );

	VCRHook_RegSetValueEx(
		m_hKey,		// handle to key
		key,	// value name
		0,			// reserved
		REG_DWORD,		// type buffer
		(LPBYTE)&value,    // data buffer
		dwSize );  // size of data buffer
}

//-----------------------------------------------------------------------------
// Purpose: Read string value from registry
// Input  : *key - 
//			*defaultValue - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CRegistry::ReadString( const char *key, const char *defaultValue /* = NULL */ )
{
	// Type of key
	DWORD dwType;        
	// Size of element data
	DWORD dwSize = 512;           

	static char value[ 512 ];

	value[0] = 0;

	if ( !m_bValid )
	{
		return defaultValue;
	}

	LONG lResult = VCRHook_RegQueryValueEx(
		m_hKey,		// handle to key
		key,	// value name
		0,			// reserved
		&dwType,    // type buffer
		(unsigned char *)value,    // data buffer
		&dwSize );  // size of data buffer

	if ( lResult != ERROR_SUCCESS ) 
	{
		return defaultValue;
	}

	if ( dwType != REG_SZ )
	{
		return defaultValue;
	}

	return value;
}

//-----------------------------------------------------------------------------
// Purpose: Save string to registry
// Input  : *key - 
//			*value - 
//-----------------------------------------------------------------------------
void CRegistry::WriteString( const char *key, const char *value )
{
	if ( !m_bValid )
	{
		return;
	}

	DWORD dwSize = (DWORD)(strlen(value) + 1);

	VCRHook_RegSetValueEx(
		m_hKey,		// handle to key
		key,	// value name
		0,			// reserved
		REG_SZ,		// type buffer
		(const LPBYTE)value,    // data buffer
		dwSize );  // size of data buffer
}




bool CRegistry::DirectInit( const char *subDirectoryUnderValve )
{
	ULONG dwDisposition;    // Type of key opening event

	char szModelKey[ 1024 ];
	V_sprintf_safe( szModelKey, "Software\\Valve\\%s", subDirectoryUnderValve );

	LONG lResult = VCRHook_RegCreateKeyEx(
		HKEY_CURRENT_USER,	// handle of open key 
		szModelKey,			// address of name of subkey to open 
		0ul,					// DWORD ulOptions,	  // reserved 
		NULL,			// Type of value
		REG_OPTION_NON_VOLATILE, // Store permanently in reg.
		KEY_ALL_ACCESS,		// REGSAM samDesired, // security access mask 
		NULL,
		&m_hKey,				// Key we are creating
		&dwDisposition );    // Type of creation

	if ( lResult != ERROR_SUCCESS )
	{
		m_bValid = false;
		return false;
	}
	
	// Success
	m_bValid = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Open default launcher key based on game directory
//-----------------------------------------------------------------------------
bool CRegistry::Init( const char *platformName )
{
	char subDir[ 512 ];
	V_sprintf_safe( subDir, "%s\\Settings", platformName );
	return DirectInit( subDir );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRegistry::Shutdown()
{
	if ( !m_bValid )
		return;

	// Make invalid
	m_bValid = false;
	VCRHook_RegCloseKey( m_hKey );
}

int64 CRegistry::ReadInt64( const char* key, int64 defaultValue )
{
	DWORD dwType;           // Type of key

	int64 value;

	if ( !m_bValid )
	{
		return defaultValue;
	}

	DWORD dwSize = sizeof( value );

	LONG lResult = VCRHook_RegQueryValueEx(
		m_hKey,		// handle to key
		key,	// value name
		0,			// reserved
		&dwType,    // type buffer
		(LPBYTE)&value,    // data buffer
		&dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return defaultValue;

	if (dwType != REG_QWORD)
		return defaultValue;

	return value;
}

void CRegistry::WriteInt64( const char* key, int64 value )
{
	if ( !m_bValid )
	{
		return;
	}

	constexpr DWORD dwSize = sizeof( value );

	VCRHook_RegSetValueEx(
		m_hKey,		// handle to key
		key,	// value name
		0,			// reserved
		REG_QWORD,		// type buffer
		(LPBYTE)&value,    // data buffer
		dwSize );  // size of data buffer
}

#else






//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRegistry::CRegistry()
{
	// Assume failure
	m_bValid	= false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRegistry::~CRegistry()
{
}

//-----------------------------------------------------------------------------
// Purpose: Read integer from registry
// Input  : *key - 
//			defaultValue - 
// Output : int
//-----------------------------------------------------------------------------
int CRegistry::ReadInt( const char *key, int defaultValue /*= 0*/ )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Save integer to registry
// Input  : *key - 
//			value - 
//-----------------------------------------------------------------------------
void CRegistry::WriteInt( const char *key, int value )
{
}

int64 CRegistry::ReadInt64( const char* key, int64 defaultValue )
{
	return 0;
}

void CRegistry::WriteInt64( const char* key, int64 value )
{
}

//-----------------------------------------------------------------------------
// Purpose: Read string value from registry
// Input  : *key - 
//			*defaultValue - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CRegistry::ReadString( const char *key, const char *defaultValue /* = NULL */ )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Save string to registry
// Input  : *key - 
//			*value - 
//-----------------------------------------------------------------------------
void CRegistry::WriteString( const char *key, const char *value )
{
}



bool CRegistry::DirectInit( const char *subDirectoryUnderValve )
{

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Open default launcher key based on game directory
//-----------------------------------------------------------------------------
bool CRegistry::Init( const char *platformName )
{
	char subDir[ 512 ];
	V_sprintf_safe( subDir, "%s\\Settings", platformName );
	return DirectInit( subDir );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRegistry::Shutdown()
{
	if ( !m_bValid )
		return;

	// Make invalid
	m_bValid = false;
}
#endif // POSIX

