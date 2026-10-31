//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "Sys_Utils.h"

#include "tier0/vcrmode.h"

#if defined( _WIN32 )
#include "winlite.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( _WIN32 )

const unsigned int SYS_NO_ERROR{ NO_ERROR };
const unsigned int SYS_ERROR_INVALID_HANDLE{ ERROR_INVALID_HANDLE };

void Sys_SetLastError( unsigned long error )
{
	::SetLastError( error );
}

unsigned long Sys_GetLastError()
{
	return ::GetLastError();
}


WHANDLE Sys_CreateMutex( const char *mutexName )
{
	return reinterpret_cast< WHANDLE >( ::CreateMutexA( nullptr, FALSE, mutexName ) );
}

void Sys_ReleaseMutex( WHANDLE mutexHandle )
{
	::ReleaseMutex( reinterpret_cast< HANDLE >( mutexHandle ) );
}


const unsigned int SYS_WAIT_OBJECT_0{ WAIT_OBJECT_0 };
const unsigned int SYS_WAIT_ABANDONED{ WAIT_ABANDONED };

unsigned int Sys_WaitForSingleObject( WHANDLE mutexHandle, int milliseconds )
{
	return VCRHook_WaitForSingleObject( reinterpret_cast< void * >( mutexHandle ), milliseconds );
}

unsigned int Sys_RegisterWindowMessage( const char *msgName )
{
	return ::RegisterWindowMessageA( msgName );
}

WHANDLE Sys_FindWindow( const char *className, const char *windowName )
{
	return reinterpret_cast< WHANDLE >( ::FindWindowA( className, windowName ) );
}

void Sys_EnumWindows( void *callbackFunction, int lparam )
{
	::EnumWindows( reinterpret_cast< WNDENUMPROC >( reinterpret_cast< uintp >( callbackFunction ) ), lparam );
}

void Sys_GetWindowText( WHANDLE wnd, char *buffer, int bufferSize )
{
	::GetWindowTextA( reinterpret_cast< HWND >( wnd ), buffer, bufferSize - 1 );
}

void Sys_PostMessage( WHANDLE wnd, unsigned int msg, unsigned int wParam, unsigned int lParam )
{
	::PostMessageA( reinterpret_cast< HWND >( wnd ), msg, wParam, lParam );
}

void Sys_SetCursorPos( int x, int y )
{
	::SetCursorPos( x, y );
}

#elif defined( POSIX )

const unsigned int SYS_NO_ERROR{ 0 };
const unsigned int SYS_ERROR_INVALID_HANDLE{ 0 };
const unsigned int SYS_WAIT_OBJECT_0{ 0 };
const unsigned int SYS_WAIT_ABANDONED{ 0 };

void Sys_SetLastError( unsigned long error )
{
	errno = error;
}

unsigned long Sys_GetLastError()
{
	return errno;
}


WHANDLE Sys_CreateMutex( const char *mutexName )
{
	AssertMsg( false, "Implement me" );
	return 0;
}

void Sys_ReleaseMutex( WHANDLE mutexHandle )
{
	AssertMsg( false, "Implement me" );
}

void Sys_PostMessage( WHANDLE wnd, unsigned int msg, unsigned int wParam, unsigned int lParam )
{
	AssertMsg( false, "Implement me" );
}

unsigned int Sys_RegisterWindowMessage( const char *msgName )
{
	AssertMsg( false, "Implement me" );
	return 0;
}

unsigned int Sys_WaitForSingleObject( WHANDLE mutexHandle, int milliseconds )
{
	return SYS_WAIT_ABANDONED;
}

void Sys_EnumWindows( void *callbackFunction, int lparam )
{
	AssertMsg( false, "Implement me" );
}

#else
#error "Please define your platform"
#endif
