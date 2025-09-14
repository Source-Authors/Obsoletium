//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Utilities for setting vproject settings
//
//===========================================================================//

#ifndef SE_PUBLIC_TIER2_VCONFIG_H_
#define SE_PUBLIC_TIER2_VCONFIG_H_

// The registry keys that vconfig uses to store the current vproject directory.
//#define VPROJECT_REG_KEY	"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"
// Moved to simply 'Environment' - we need to set in HKEY_CURRENT_USER and not local machine, to avoid security issues in vista!
#define VPROJECT_REG_KEY "Environment"

// For accessing the environment variables we store the current vproject in.
void SetVConfigRegistrySetting( const char *pName, const char *pValue, bool bNotify = true );
[[nodiscard]] bool GetVConfigRegistrySetting( const char *pName, char *pReturn, unsigned long size );

template<unsigned long size>
[[nodiscard]] bool GetVConfigRegistrySetting( const char *pName, char (&pReturn)[size] )
{
	return GetVConfigRegistrySetting( pName, pReturn, size );
}

#ifdef _WIN32
[[nodiscard]] bool RemoveObsoleteVConfigRegistrySetting( const char *pValueName, char *pOldValue = nullptr, unsigned long size = 0 ); 

template<unsigned long size>
[[nodiscard]] bool RemoveObsoleteVConfigRegistrySetting( const char *pValueName, char (&pOldValue)[size] )
{
	return RemoveObsoleteVConfigRegistrySetting( pValueName, pOldValue, size );
}
#endif

bool ConvertObsoleteVConfigRegistrySetting( const char *pValueName );


#endif  // !SE_PUBLIC_TIER2_VCONFIG_H_
