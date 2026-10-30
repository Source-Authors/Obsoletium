// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"
#include "tier0/systeminformation.h"

#ifdef IS_WINDOWS_PC

#include "winlite.h"
#include <cstring>
#include <cstddef>

//
//	NtQuerySystemInformation is exported by ntdll.dll but isn't declared in the
//	regular SDK headers, so declare just what we need here.
//
using NTSTATUS_t = LONG;
using PFN_NtQuerySystemInformation_t = NTSTATUS_t( WINAPI * )(
ULONG  SystemInformationClass,
PVOID  SystemInformation,
ULONG  SystemInformationLength,
PULONG ReturnLength );

constexpr inline ULONG      kSystemPerformanceInformation = 2;
constexpr inline NTSTATUS_t kStatusInfoLengthMismatch = (NTSTATUS_t)0xC0000004L;

//
//	Leading portion of SYSTEM_PERFORMANCE_INFORMATION, up to and including the
//	fields we read. The full structure has grown across Windows versions, but
//	this prefix has been stable since NT. It contains no pointers, so the layout
//	is identical on 32-bit and 64-bit Windows.
//
struct SystemPerformanceInformationPrefix_t
{
	LARGE_INTEGER IdleProcessTime;
	LARGE_INTEGER IoReadTransferCount;
	LARGE_INTEGER IoWriteTransferCount;
	LARGE_INTEGER IoOtherTransferCount;
	ULONG         IoReadOperationCount;
	ULONG         IoWriteOperationCount;
	ULONG         IoOtherOperationCount;
	ULONG         AvailablePages;
	ULONG         CommittedPages;
	ULONG         CommitLimit;
	ULONG         PeakCommitment;
	ULONG         PageFaultCount;
	ULONG         CopyOnWriteCount;
	ULONG         TransitionCount;
	ULONG         CacheTransitionCount;
	ULONG         DemandZeroCount;
	ULONG         PageReadCount;
	ULONG         PageReadIoCount;
	ULONG         CacheReadCount;
	ULONG         CacheIoCount;
	ULONG         DirtyPagesWriteCount;
	ULONG         DirtyWriteIoCount;
	ULONG         MappedPagesWriteCount;
	ULONG         MappedWriteIoCount;
	ULONG         PagedPoolPages;
	ULONG         NonPagedPoolPages;
	ULONG         PagedPoolAllocs;
	ULONG         PagedPoolFrees;
	ULONG         NonPagedPoolAllocs;
	ULONG         NonPagedPoolFrees;
	ULONG         FreeSystemPtes;
	ULONG         ResidentSystemCodePage;
	ULONG         TotalSystemDriverPages;
	ULONG         TotalSystemCodePages;
	ULONG         NonPagedPoolLookasideHits;
	ULONG         PagedPoolLookasideHits;
	ULONG         AvailablePagedPoolPages;
};

constexpr inline ULONG kcbRequiredPerfInfo =
(ULONG)( offsetof( SystemPerformanceInformationPrefix_t, AvailablePagedPoolPages ) + sizeof( ULONG ) );

//
//	Queries SystemPerformanceInformation into pOut.
//	The kernel rejects buffers smaller than its own (version dependent) structure,
//	so query into a generous buffer and grow it if the kernel asks for more.
//
static SYSTEM_CALL_RESULT_t QueryPerformanceInformation( SystemPerformanceInformationPrefix_t *pOut )
{
	HMODULE hNtDll = GetModuleHandleW( L"ntdll.dll" );
	if ( !hNtDll )
		return SYSCALL_NODLL;

	const auto pfnNtQuerySystemInformation =
	(PFN_NtQuerySystemInformation_t)GetProcAddress( hNtDll, "NtQuerySystemInformation" );
	if ( !pfnNtQuerySystemInformation )
		return SYSCALL_NOPROC;

	// 8-byte aligned stack buffer, comfortably larger than any shipped version of the structure.
	ULONGLONG stackBuffer[128];
	void     *pBuffer = stackBuffer;
	ULONG     cbBuffer = sizeof( stackBuffer );
	HANDLE    hHeap = GetProcessHeap();

	SYSTEM_CALL_RESULT_t result = SYSCALL_FAILED;

	for ( int nAttempt = 0; nAttempt < 4; ++nAttempt )
	{
		ULONG      cbReturned = 0;
		NTSTATUS_t status = pfnNtQuerySystemInformation( kSystemPerformanceInformation, pBuffer, cbBuffer, &cbReturned );

		if ( status >= 0 ) // NT_SUCCESS
		{
			if ( cbReturned >= kcbRequiredPerfInfo )
			{
				memcpy( pOut, pBuffer, sizeof( *pOut ) );
				result = SYSCALL_SUCCESS;
			}
			break;
		}

		if ( status != kStatusInfoLengthMismatch || cbReturned <= cbBuffer )
			break;

		// Kernel wants a bigger buffer. Use the process heap directly rather than
		// malloc, since tier0 may be the one providing the allocator.
		if ( pBuffer != stackBuffer )
			HeapFree( hHeap, 0, pBuffer );

		cbBuffer = cbReturned;
		pBuffer = HeapAlloc( hHeap, 0, cbBuffer );
		if ( !pBuffer )
			return SYSCALL_FAILED;
	}

	if ( pBuffer != stackBuffer )
		HeapFree( hHeap, 0, pBuffer );

	return result;
}

//
//	Plat_GetMemPageSize
//		Returns the size of a memory page in kilobytes.
//
unsigned long Plat_GetMemPageSize()
{
	static unsigned long s_nPageSizeKB = 0; // Benign race: every thread computes the same value

	if ( !s_nPageSizeKB )
	{
		SYSTEM_INFO si;
		GetSystemInfo( &si );
		unsigned long nKB = si.dwPageSize / 1024;
		s_nPageSizeKB = nKB ? nKB : 4;
	}

	return s_nPageSizeKB;
}

//
//	Plat_GetPagedPoolInfo
//		Fills in the paged pool info structure if successful.
//		Counts are in pages of Plat_GetMemPageSize() kilobytes.
//
SYSTEM_CALL_RESULT_t Plat_GetPagedPoolInfo( PAGED_POOL_INFO_t *pPPI )
{
	if ( !pPPI )
		return SYSCALL_FAILED;

	BitwiseClear( *pPPI );

	SystemPerformanceInformationPrefix_t spi;
	SYSTEM_CALL_RESULT_t                 result = QueryPerformanceInformation( &spi );
	if ( result != SYSCALL_SUCCESS )
		return result;

	pPPI->numPagesUsed = spi.PagedPoolPages;
	pPPI->numPagesFree = spi.AvailablePagedPoolPages;

	return SYSCALL_SUCCESS;
}


#else // POSIX (Linux, macOS, ...)


#include <unistd.h>

//
//	Plat_GetMemPageSize
//		Returns the size of a memory page in kilobytes.
//
unsigned long Plat_GetMemPageSize()
{
	static unsigned long s_nPageSizeKB = 0; // Benign race: every thread computes the same value

	if ( !s_nPageSizeKB )
	{
		long          cbPage = sysconf( _SC_PAGESIZE );
		unsigned long nKB = ( cbPage > 0 ) ? static_cast<unsigned long>(cbPage) / 1024 : 0;
		s_nPageSizeKB = nKB ? nKB : 4;
	}

	return s_nPageSizeKB;
}

//
//	Plat_GetPagedPoolInfo
//		Fills in the paged pool info structure if successful.
//		The paged pool is a Windows kernel concept; Linux and macOS kernel memory
//		is not pageable, so there is no equivalent to report.
//
SYSTEM_CALL_RESULT_t Plat_GetPagedPoolInfo( PAGED_POOL_INFO_t *pPPI )
{
	if ( !pPPI )
		return SYSCALL_FAILED;

	BitwiseClear( *pPPI );
	return SYSCALL_UNSUPPORTED;
}


#endif
