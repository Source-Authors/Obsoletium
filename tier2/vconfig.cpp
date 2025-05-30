//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Utilities for setting vproject settings
//
//===========================================================================//

#include "tier2/vconfig.h"

#ifdef _WIN32
#include "winlite.h"
#endif


#ifdef _WIN32
//-----------------------------------------------------------------------------
// Purpose: Returns the string value of a registry key
// Input  : *pName - name of the subKey to read
//			*pReturn - string buffer to receive read string
//			size - size of specified buffer
//-----------------------------------------------------------------------------
bool GetVConfigRegistrySetting( const char *pName, char *pReturn, unsigned long size )
{
	// dimhotepus: Null terminate.
	if (size)
		pReturn[0] = '\0';

	// Open the key
	HKEY hregkey; 
	// Changed to HKEY_CURRENT_USER from HKEY_LOCAL_MACHINE
	if ( RegOpenKeyEx( HKEY_CURRENT_USER, VPROJECT_REG_KEY, 0, KEY_QUERY_VALUE, &hregkey ) != ERROR_SUCCESS )
		return false;
	
	// Get the value
	DWORD dwSize = size;
	if ( RegQueryValueEx( hregkey, pName, NULL, NULL,(LPBYTE) pReturn, &dwSize ) != ERROR_SUCCESS )
	{
		// dimhotepus: Do not leak the key. 
		RegCloseKey( hregkey );
		return false;
	}
	
	// Close the key
	RegCloseKey( hregkey );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Sends a global system message to alert programs to a changed environment variable
//-----------------------------------------------------------------------------
static void NotifyVConfigRegistrySettingChanged( void )
{
	DWORD_PTR dwReturnValue = 0;
	
	// Propagate changes so that environment variables takes immediate effect!
	// dimhotepus: Use const instead of magic value.
	SendMessageTimeout( HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM) VPROJECT_REG_KEY, SMTO_ABORTIFHUNG, 5000, &dwReturnValue );
}

//-----------------------------------------------------------------------------
// Purpose: Set the registry entry to a string value, under the given subKey
// Input  : *pName - name of the subKey to set
//			*pValue - string value
//-----------------------------------------------------------------------------
void SetVConfigRegistrySetting( const char *pName, const char *pValue, bool bNotify )
{
	HKEY hregkey; 

	// Changed to HKEY_CURRENT_USER from HKEY_LOCAL_MACHINE
	// Open the key
	if ( RegCreateKeyEx( 
		HKEY_CURRENT_USER,		// base key
		VPROJECT_REG_KEY,		// subkey
		0,						// reserved
		0,						// lpClass
		0,						// options
		(REGSAM)KEY_ALL_ACCESS,	// access desired
		NULL,					// security attributes
		&hregkey,				// result
		NULL					// tells if it created the key or not (which we don't care)
		) != ERROR_SUCCESS )
	{
		return;
	}
	
	// Set the value to the string passed in
	const DWORD nType = strchr( pValue, '%' ) ? REG_EXPAND_SZ : REG_SZ;
	
	// Notify other programs
	// dimhotepus: Notify oly when value is set.
	if ( RegSetValueEx( hregkey, pName, 0, nType, (const unsigned char *)pValue, static_cast<DWORD>( strlen(pValue) ) ) == ERROR_SUCCESS &&
		 bNotify )
	{
		NotifyVConfigRegistrySettingChanged();
	}
	
	// Close the key
	RegCloseKey( hregkey );
}

//-----------------------------------------------------------------------------
// Purpose: Removes the obsolete user keyvalue
// Input  : *pName - name of the subKey to set
//			*pValue - string value
//-----------------------------------------------------------------------------
bool RemoveObsoleteVConfigRegistrySetting( const char *pValueName, char *pOldValue, unsigned long size )
{
	// Open the key
	HKEY hregkey; 
	// dimhotepus: Use const instead of magic value.
	if ( RegOpenKeyEx( HKEY_CURRENT_USER, VPROJECT_REG_KEY, 0, KEY_ALL_ACCESS, &hregkey ) != ERROR_SUCCESS )
		return false;

	// Return the old state if they've requested it
	if ( pOldValue != NULL )
	{
		DWORD dwSize = size;

		// Get the value
		if ( RegQueryValueEx( hregkey, pValueName, NULL, NULL, (LPBYTE) pOldValue, &dwSize ) != ERROR_SUCCESS )
		{
			// dimhotepus: Do not leak the key. 
			RegCloseKey( hregkey );
			return false;
		}
	}
	
	// Remove the value
	if ( RegDeleteValue( hregkey, pValueName ) != ERROR_SUCCESS )
	{
		// dimhotepus: Do not leak the key. 
		RegCloseKey( hregkey );
		return false;
	}

	// Close the key
	RegCloseKey( hregkey );

	// Notify other programs
	NotifyVConfigRegistrySettingChanged();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Take a user-defined environment variable and swap it out for the internally used one
//-----------------------------------------------------------------------------
bool ConvertObsoleteVConfigRegistrySetting( const char *pValueName )
{
	char szValue[MAX_PATH];
	if ( RemoveObsoleteVConfigRegistrySetting( pValueName, szValue ) )
	{
		// Set it up the correct way
		SetVConfigRegistrySetting( pValueName, szValue );
		return true;
	}

	return false;
}
#endif
