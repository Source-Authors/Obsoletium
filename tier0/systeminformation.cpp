// opyright Valve Corporation, All rights reserved.

#include "pch_tier0.h"
#include "tier0/systeminformation.h"

#ifdef IS_WINDOWS_PC

//
//	Plat_GetMemPageSize
//		Returns the size of a memory page in bytes.
//
unsigned long Plat_GetMemPageSize()
{
	return 4;	// On 32-bit systems memory page size is 4 KiB
}

//
//	Plat_GetPagedPoolInfo
//		Fills in the paged pool info structure if successful.
//
SYSTEM_CALL_RESULT_t Plat_GetPagedPoolInfo( PAGED_POOL_INFO_t *pPPI )
{
	memset( pPPI, 0, sizeof( *pPPI ) );
	return SYSCALL_UNSUPPORTED;
}


#else


//
//	Plat_GetMemPageSize
//		Returns the size of a memory page in bytes.
//
unsigned long Plat_GetMemPageSize()
{
	return 4;	// Assume unknown page size is 4 KiB
}

//
//	Plat_GetPagedPoolInfo
//		Fills in the paged pool info structure if successful.
//
SYSTEM_CALL_RESULT_t Plat_GetPagedPoolInfo( PAGED_POOL_INFO_t *pPPI )
{
	memset( pPPI, 0, sizeof( *pPPI ) );
	return SYSCALL_UNSUPPORTED;
}


#endif
