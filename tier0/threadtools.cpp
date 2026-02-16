// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include <thread>

#include "tier1/strtools.h"
#include "tier0/dynfunction.h"
#ifdef _WIN32
	#include "winlite.h"
	#include <process.h>

	#include "scoped_dll.h"

#elif defined(POSIX)

#if !defined(OSX)
	#include <sys/fcntl.h>
	#include <sys/unistd.h>
	#define sem_unlink( arg )
	#define OS_TO_PTHREAD(x) (x)
#else
	#define pthread_yield pthread_yield_np
	#include <mach/thread_act.h>
	#include <mach/mach.h>
	#define OS_TO_PTHREAD(x) pthread_from_mach_thread_np( x )
#endif // !OSX

#ifdef LINUX
#include <dlfcn.h> // RTLD_NEXT
#endif

typedef int (*PTHREAD_START_ROUTINE)(
    void *lpThreadParameter
    );
typedef PTHREAD_START_ROUTINE LPTHREAD_START_ROUTINE;
#include <sched.h>
#include <exception>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#define GetLastError() errno
typedef void *LPVOID;
#endif

#include "tier0/valve_minmax_off.h"
#include <memory>
#include "tier0/valve_minmax_on.h"

#include "tier0/threadtools.h"
#include "tier0/vcrmode.h"

#ifdef _X360
#include "xbox/xbox_win32stubs.h"
#endif

#include "tier0/vprof_telemetry.h"

// Must be last header...
#include "tier0/memdbgon.h"

#define THREADS_DEBUG 1

// Need to ensure initialized before other clients call in for main thread ID
#ifdef _WIN32
#pragma warning(disable:4073)
#pragma init_seg(lib)
#endif

#ifdef _WIN32
ASSERT_INVARIANT(TT_SIZEOF_CRITICALSECTION == sizeof(CRITICAL_SECTION));
ASSERT_INVARIANT(TT_INFINITE == INFINITE);
#endif

//-----------------------------------------------------------------------------
// Simple thread functions. 
// Because _beginthreadex uses stdcall, we need to convert to cdecl
//-----------------------------------------------------------------------------
struct ThreadProcInfo_t
{
	ThreadProcInfo_t( ThreadFunc_t pfnThread_, void *pParam_ )
	  : pfnThread( pfnThread_ ),
		pParam( pParam_ )
	{
	}
	
	ThreadFunc_t pfnThread;
	void *		 pParam;
};

//---------------------------------------------------------

#ifdef _WIN32
static unsigned __stdcall ThreadProcConvert( void *pParam )
#elif defined(POSIX)
static void *ThreadProcConvert( void *pParam )
#else
#error "Please define your platform"
#endif
{
	ThreadProcInfo_t info = *((ThreadProcInfo_t *)pParam);
	delete ((ThreadProcInfo_t *)pParam);
#ifdef _WIN32
	return (*info.pfnThread)(info.pParam);
#elif defined(POSIX)
	return (void *)(*info.pfnThread)(info.pParam);
#else
#error "Please define your platform"
#endif
}


//---------------------------------------------------------

ThreadHandle_t CreateSimpleThread( ThreadFunc_t pfnThread, void *pParam, ThreadId_t *pID, unsigned stackSize )
{
#ifdef _WIN32
	ThreadId_t idIgnored;
	if ( !pID )
		pID = &idIgnored;
	HANDLE h = VCRHook_CreateThread(nullptr, stackSize, reinterpret_cast<void*>(ThreadProcConvert), new ThreadProcInfo_t( pfnThread, pParam ), CREATE_SUSPENDED, pID);
	if ( h != INVALID_HANDLE_VALUE )
	{
		Plat_ApplyHardwareDataBreakpointsToNewThread( *pID );
		ResumeThread( h );
	}
	return (ThreadHandle_t)h;
#elif defined(POSIX)
	pthread_t tid;

	// If we need to create threads that are detached right out of the gate, we would need to do something like this:
	//   pthread_attr_t attr;
	//   int rc = pthread_attr_init(&attr);
	//   rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	//   ... pthread_create( &tid, &attr, ... ) ...
	//   rc = pthread_attr_destroy(&attr);
	//   ... pthread_join will now fail

	int ret = pthread_create( &tid, NULL, ThreadProcConvert, new ThreadProcInfo_t( pfnThread, pParam ) );
	if ( ret )
	{
		// There are only PTHREAD_THREADS_MAX number of threads, and we're probably leaking handles if ret == EAGAIN here?
		Error( "CreateSimpleThread: pthread_create failed. Someone not calling pthread_detach() or pthread_join. Ret:%d\n", ret );
	}
	if ( pID )
		*pID = (ThreadId_t)tid;
	Plat_ApplyHardwareDataBreakpointsToNewThread( (long unsigned int)tid );
	return (ThreadHandle_t)tid;
#endif
}

ThreadHandle_t CreateSimpleThread( ThreadFunc_t pfnThread, void *pParam, unsigned stackSize )
{
	return CreateSimpleThread( pfnThread, pParam, nullptr, stackSize );
}

PLATFORM_INTERFACE void ThreadDetach( [[maybe_unused]] ThreadHandle_t hThread )
{
#if defined( POSIX )
	// The resources of this thread will be freed immediately when it terminates,
	//  instead of waiting for another thread to perform PTHREAD_JOIN.
	pthread_t tid = ( pthread_t )hThread;

	pthread_detach( tid );
#endif
}

bool ReleaseThreadHandle( ThreadHandle_t hThread )
{
#ifdef _WIN32
	return ( CloseHandle( hThread ) != 0 );
#else
	return true;
#endif
}

//-----------------------------------------------------------------------------
//
// Wrappers for other simple threading operations
//
//-----------------------------------------------------------------------------

void ThreadSleep(unsigned nMilliseconds)
{
	if ( nMilliseconds == 0 )
	{
		// dimhotepus: Pause a bit.
		ThreadPause();
	}
	else
	{
		// dimhotepus: Do not use std thread APIs. Causes issues with ASAN.
#ifdef _WIN32
		Sleep(nMilliseconds);
#else
		usleep(nMilliseconds * 1000);
#endif
	}
}

//-----------------------------------------------------------------------------

#ifndef ThreadGetCurrentId
ThreadId_t ThreadGetCurrentId()
{
#ifdef _WIN32
	return GetCurrentThreadId();
#elif defined(POSIX)
	return (ThreadId_t)pthread_self();
#endif
}
#endif

//-----------------------------------------------------------------------------
ThreadHandle_t ThreadGetCurrentHandle()
{
#ifdef _WIN32
	return (ThreadHandle_t)GetCurrentThread();
#elif defined(POSIX)
	return (ThreadHandle_t)pthread_self();
#endif
}

// On PS3, this will return true for zombie threads
bool ThreadIsThreadIdRunning( ThreadId_t uThreadId )
{
#ifdef _WIN32
	bool bRunning = true;
	HANDLE hThread = ::OpenThread( THREAD_QUERY_INFORMATION , false, uThreadId );
	if ( hThread )
	{
		DWORD dwExitCode;
		if( !::GetExitCodeThread( hThread, &dwExitCode ) || dwExitCode != STILL_ACTIVE )
			bRunning = false;

		CloseHandle( hThread );
	}
	else
	{
		bRunning = false;
	}
	return bRunning;
#elif defined( _PS3 )
	
	// will return CELL_OK for zombie threads
	int priority;
	return (sys_ppu_thread_get_priority( uThreadId, &priority ) == CELL_OK );

#elif defined(POSIX)
	pthread_t thread = OS_TO_PTHREAD(uThreadId);
	if ( thread )
	{
		int iResult = pthread_kill( thread, 0 );
		if ( iResult == 0 )
			return true;
	}
	else
	{
		// We really ought not to be passing NULL in to here
		AssertMsg( false, "ThreadIsThreadIdRunning received a null thread ID" );
	}

	return false;
#endif
}

//-----------------------------------------------------------------------------

int ThreadGetPriority( ThreadHandle_t hThread )
{
	if ( !hThread )
	{
		hThread = ThreadGetCurrentHandle();
	}

#ifdef _WIN32
	return ::GetThreadPriority( (HANDLE)hThread );
#else
	struct sched_param thread_param;
	int policy;
	pthread_getschedparam( (pthread_t)hThread, &policy, &thread_param );
	return thread_param.sched_priority;
#endif
}

//-----------------------------------------------------------------------------

bool ThreadSetPriority( ThreadHandle_t hThread, int priority )
{
	if ( !hThread )
	{
		hThread = ThreadGetCurrentHandle();
	}

#ifdef _WIN32
	return ( SetThreadPriority(hThread, priority) != 0 );
#elif defined(POSIX)
	struct sched_param thread_param; 
	thread_param.sched_priority = priority; 
	pthread_setschedparam( (pthread_t)hThread, SCHED_OTHER, &thread_param );
	return true;
#endif
}

//-----------------------------------------------------------------------------

void ThreadSetAffinity( ThreadHandle_t hThread, intp nAffinityMask )
{
	if ( !hThread )
	{
		hThread = ThreadGetCurrentHandle();
	}

#ifdef _WIN32
	SetThreadAffinityMask( hThread, nAffinityMask );
#elif defined(POSIX)
// 	cpu_set_t cpuSet;
// 	CPU_ZERO( cpuSet );
// 	for( int i = 0 ; i < 32; i++ )
// 	  if ( nAffinityMask & ( 1 << i ) )
// 	    CPU_SET( cpuSet, i );
// 	sched_setaffinity( hThread, sizeof( cpuSet ), &cpuSet );
#endif

}

//-----------------------------------------------------------------------------

ThreadId_t InitMainThread()
{
#ifndef LINUX
	// Skip doing the setname on Linux for the main thread. Here is why...

	// From Pierre-Loup e-mail about why pthread_setname_np() on the main thread
	// in Linux will cause some tools to display "MainThrd" as the executable name:
	//
	// You have two things in procfs, comm and cmdline.  Each of the threads have
	// a different `comm`, which is the value you set through pthread_setname_np
	// or prctl(PR_SET_NAME).  Top can either display cmdline or comm; it
	// switched to display comm by default; htop still displays cmdline by
	// default. Top -c will output cmdline rather than comm.
	// 
	// If you press 'H' while top is running it will display each thread as a
	// separate process, so you will have different entries for MainThrd,
	// MatQueue0, etc with their own CPU usage.  But when that mode isn't enabled
	// it just displays the 'comm' name from the first thread.
	ThreadSetDebugName( "MainThrd" );
#endif

#ifdef _WIN32
	return ThreadGetCurrentId();
#elif defined(POSIX)
	return (ThreadId_t)pthread_self();
#endif
}

ThreadId_t g_ThreadMainThreadID = InitMainThread();

bool ThreadInMainThread()
{
	return ( ThreadGetCurrentId() == g_ThreadMainThreadID );
}

//-----------------------------------------------------------------------------
void DeclareCurrentThreadIsMainThread()
{
	g_ThreadMainThreadID = ThreadGetCurrentId();
}

bool ThreadJoin( ThreadHandle_t hThread, unsigned timeout )
{
	// You should really never be calling this with a NULL thread handle. If you
	// are then that probably implies a race condition or threading misunderstanding.
	Assert( hThread );
	if ( !hThread )
	{
		return false;
	}

#ifdef _WIN32
	DWORD dwWait = VCRHook_WaitForSingleObject((HANDLE)hThread, timeout);
	if ( dwWait == WAIT_TIMEOUT)
		return false;
	if ( dwWait != WAIT_OBJECT_0 && ( dwWait != WAIT_FAILED && GetLastError() != 0 ) )
	{
		Assert( 0 );
		return false;
	}
#elif defined(POSIX)
	if ( pthread_join( (pthread_t)hThread, NULL ) != 0 )
		return false;
#endif
	return true;
}

#ifdef RAD_TELEMETRY_ENABLED
void TelemetryThreadSetDebugName( ThreadId_t id, const char *pszName );
#endif

//-----------------------------------------------------------------------------

void ThreadSetDebugName( ThreadId_t id, const char *pszName )
{
	if( !pszName )
		return;

#ifdef RAD_TELEMETRY_ENABLED
	TelemetryThreadSetDebugName( id, pszName );
#endif

#ifdef _WIN32
	// dimhotepus: Always set thread debug name, not just for debug session.
	{
		HANDLE handle = OpenThread( STANDARD_RIGHTS_READ | THREAD_SET_INFORMATION,
			FALSE,
			id != std::numeric_limits<ThreadId_t>::max() ? id : GetThreadId( GetCurrentThread() ) );
		if ( handle )
		{
			const size_t wcharsNeeded = mbstowcs( nullptr, pszName, INT_MAX );
			const size_t descriptionSize = (wcharsNeeded + 1) * sizeof(wchar_t);
			auto *description = static_cast<wchar_t*>( stackalloc( descriptionSize ) );

			[[maybe_unused]] const size_t wcharsConverted = mbstowcs( description, pszName, descriptionSize );
			Assert( wcharsNeeded == wcharsConverted );
			description[descriptionSize / sizeof(wchar_t) - 1] = L'\0';

			::SetThreadDescription( handle, description );

			::CloseHandle( handle );
		}
	}
#elif defined( _LINUX )
	// As of glibc v2.12, we can use pthread_setname_np.
	if ( id == std::numeric_limits<ThreadId_t>::max() )
		id = pthread_self();

	// The thread name is a meaningful C language string, whose length is
	// restricted to 16 characters, including the terminating null byte ('\0').
	char szThreadName[ 16 ];
	strncpy( szThreadName, pszName, std::size( szThreadName ) );
	szThreadName[ std::size( szThreadName ) - 1 ] = 0;

	pthread_setname_np( id, szThreadName );
#elif defined( OSX )
	// dimhotepus: MacOS only supports set name for current thread.
	if ( id == ThreadGetCurrentId() || id == std::numeric_limits<ThreadId_t>::max() )
	{
		// The thread name is a meaningful C language string, whose length is
		// restricted to 64 characters, including the terminating null byte ('\0').
		char szThreadName[ 64 ];
		strncpy( szThreadName, pszName, std::size( szThreadName ) );
		szThreadName[ std::size( szThreadName ) - 1 ] = 0;

		pthread_setname_np( szThreadName );
	}
#endif
}


//-----------------------------------------------------------------------------

#ifdef _WIN32
ASSERT_INVARIANT( TW_FAILED == WAIT_FAILED );
ASSERT_INVARIANT( TW_TIMEOUT  == WAIT_TIMEOUT );
ASSERT_INVARIANT( WAIT_OBJECT_0 == 0 );

int ThreadWaitForObjects( int nEvents, const HANDLE *pHandles, bool bWaitAll, unsigned timeout )
{
	return VCRHook_WaitForMultipleObjects( nEvents, pHandles, bWaitAll, timeout );
}
#endif


//-----------------------------------------------------------------------------
// Used to thread LoadLibrary on the 360
//-----------------------------------------------------------------------------
static ThreadedLoadLibraryFunc_t s_ThreadedLoadLibraryFunc = nullptr;
PLATFORM_INTERFACE void SetThreadedLoadLibraryFunc( ThreadedLoadLibraryFunc_t func )
{
	s_ThreadedLoadLibraryFunc = func;
}

PLATFORM_INTERFACE ThreadedLoadLibraryFunc_t GetThreadedLoadLibraryFunc()
{
	return s_ThreadedLoadLibraryFunc;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

CThreadSyncObject::CThreadSyncObject()
#ifdef _WIN32
  : m_hSyncObject( nullptr ), m_bCreatedHandle(false)
#elif defined(POSIX)
  : m_bInitalized( false )
#endif
{
}

//---------------------------------------------------------

CThreadSyncObject::~CThreadSyncObject()
{
#ifdef _WIN32
   if ( m_hSyncObject && m_bCreatedHandle )
   {
      if ( !CloseHandle(m_hSyncObject) )
	  {
		  Assert( 0 );
	  }
   }
#elif defined(POSIX)
   if ( m_bInitalized )
   {
	pthread_cond_destroy( &m_Condition );
        pthread_mutex_destroy( &m_Mutex );
	m_bInitalized = false;
   }
#endif
}

//---------------------------------------------------------

bool CThreadSyncObject::operator!() const
{
#ifdef _WIN32
   return !m_hSyncObject;
#elif defined(POSIX)
   return !m_bInitalized;
#endif
}

//---------------------------------------------------------

void CThreadSyncObject::AssertUseable()
{
#ifdef THREADS_DEBUG
#ifdef _WIN32
   AssertMsg( m_hSyncObject, "Thread synchronization object is unuseable" );
#elif defined(POSIX)
   AssertMsg( m_bInitalized, "Thread synchronization object is unuseable" );
#endif
#endif
}

//---------------------------------------------------------

bool CThreadSyncObject::Wait( uint32 dwTimeout )
{
#ifdef THREADS_DEBUG
   AssertUseable();
#endif
#ifdef _WIN32
   return ( VCRHook_WaitForSingleObject( m_hSyncObject, dwTimeout ) == WAIT_OBJECT_0 );
#elif defined(POSIX)
    pthread_mutex_lock( &m_Mutex );
    bool bRet = false;
    if ( m_cSet > 0 )
    {
		bRet = true;
		m_bWakeForEvent = false;
    }
    else
    {
		volatile int ret = 0;

		while ( !m_bWakeForEvent && ret != ETIMEDOUT )
		{
			struct timeval tv;
			gettimeofday( &tv, NULL );
			volatile struct timespec tm;
			
			uint64 actualTimeout = dwTimeout;
			
			if ( dwTimeout == TT_INFINITE && m_bManualReset )
				actualTimeout = 10; // just wait 10 msec at most for manual reset events and loop instead
				
			volatile uint64 nNanoSec = (uint64)tv.tv_usec*1000 + (uint64)actualTimeout*1000000;
			tm.tv_sec = tv.tv_sec + nNanoSec /1000000000;
			tm.tv_nsec = nNanoSec % 1000000000;

			do
			{   
				ret = pthread_cond_timedwait( &m_Condition, &m_Mutex, (const timespec *)&tm );
			} 
			while( ret == EINTR );

			bRet = ( ret == 0 );
			
			if ( m_bManualReset )
			{
				if ( m_cSet )
					break;
				if ( dwTimeout == TT_INFINITE && ret == ETIMEDOUT )
					ret = 0; // force the loop to spin back around
			}
		}
		
		if ( bRet )
			m_bWakeForEvent = false;
    }
    if ( !m_bManualReset && bRet )
		m_cSet = 0;
    pthread_mutex_unlock( &m_Mutex );
    return bRet;
#endif
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

CThreadEvent::CThreadEvent( bool bManualReset )
{
#ifdef _WIN32
    m_hSyncObject = CreateEvent( nullptr, bManualReset, FALSE, nullptr );
	m_bCreatedHandle = true;
    AssertMsg1(m_hSyncObject, "Failed to create event: %s",
		std::system_category().message(::GetLastError()).c_str() );
#elif defined( POSIX )
    pthread_mutexattr_t Attr;
    pthread_mutexattr_init( &Attr );
    pthread_mutex_init( &m_Mutex, &Attr );
    pthread_mutexattr_destroy( &Attr );
    pthread_cond_init( &m_Condition, NULL );
    m_bInitalized = true;
    m_cSet = 0;
	m_bWakeForEvent = false;
    m_bManualReset = bManualReset;
#else
#error "Implement me"
#endif
}

#ifdef _WIN32
CThreadEvent::CThreadEvent( HANDLE hHandle )
{
	m_hSyncObject = hHandle;
	m_bCreatedHandle = false;
	AssertMsg(m_hSyncObject, "Null event passed into constructor" );
}
#endif

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------


//---------------------------------------------------------

bool CThreadEvent::Set()
{
   AssertUseable();
#ifdef _WIN32
   return ( SetEvent( m_hSyncObject ) != 0 );
#elif defined(POSIX)
    pthread_mutex_lock( &m_Mutex );
    m_cSet = 1;
	m_bWakeForEvent = true;
    int ret = pthread_cond_signal( &m_Condition );
    pthread_mutex_unlock( &m_Mutex );
    return ret == 0;
#endif
}

//---------------------------------------------------------

bool CThreadEvent::Reset()
{
#ifdef THREADS_DEBUG
   AssertUseable();
#endif
#ifdef _WIN32
   return ( ResetEvent( m_hSyncObject ) != 0 );
#elif defined(POSIX)
	pthread_mutex_lock( &m_Mutex );
	m_cSet = 0;
	m_bWakeForEvent = false;
	pthread_mutex_unlock( &m_Mutex );
	return true; 
#endif
}

//---------------------------------------------------------

bool CThreadEvent::Check()
{
#ifdef THREADS_DEBUG
   AssertUseable();
#endif
	return Wait( 0 );
}



bool CThreadEvent::Wait( uint32 dwTimeout )
{
	return CThreadSyncObject::Wait( dwTimeout );
}

#ifdef _WIN32
//-----------------------------------------------------------------------------
//
// CThreadSemaphore
//
// To get Posix implementation, try http://www-128.ibm.com/developerworks/eserver/library/es-win32linux-sem.html
//
//-----------------------------------------------------------------------------

CThreadSemaphore::CThreadSemaphore( long initialValue, long maxValue )
{
	if ( maxValue )
	{
		AssertMsg( maxValue > 0, "Invalid max value for semaphore" );
		AssertMsg( initialValue >= 0 && initialValue <= maxValue, "Invalid initial value for semaphore" );

		m_hSyncObject = CreateSemaphore( nullptr, initialValue, maxValue, nullptr );

		AssertMsg1(m_hSyncObject, "Failed to create semaphore: %s",
			std::system_category().message(::GetLastError()).c_str() );
	}
	else
	{
		m_hSyncObject = nullptr;
	}
}

//---------------------------------------------------------

bool CThreadSemaphore::Release( long releaseCount, long *pPreviousCount )
{
#ifdef THRDTOOL_DEBUG
   AssertUseable();
#endif
   return ( ReleaseSemaphore( m_hSyncObject, releaseCount, pPreviousCount ) != 0 );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

CThreadFullMutex::CThreadFullMutex( bool bEstablishInitialOwnership, const char *pszName )
{
   m_hSyncObject = CreateMutex( nullptr, bEstablishInitialOwnership, pszName );

   AssertMsg1( m_hSyncObject, "Failed to create mutex: %s",
	   std::system_category().message(::GetLastError()).c_str() );
}

//---------------------------------------------------------

bool CThreadFullMutex::Release()
{
#ifdef THRDTOOL_DEBUG
   AssertUseable();
#endif
   return ( ReleaseMutex( m_hSyncObject ) != 0 );
}

#endif

#ifdef _WIN32

// dimhotepus: TlsGetValue2 and TlsGetValue have same signatures,
// so use TlsGetValue as TlsGetValue2 is from Windows 11 24H2 SDK not installed for some users.
using TlsGetValue2Function = decltype(&TlsGetValue);

static const source::ScopedDll scopedKernel32Dll{ "kernel32.dll", LOAD_LIBRARY_SEARCH_SYSTEM32 };
// dimhotepus: TlsGetValue2 doesn't set GetLastError (involves thread local storage) so is faster.
const auto [tlsGetValue2, tlsRc] = scopedKernel32Dll.GetFunction<TlsGetValue2Function>("TlsGetValue2");
static const TlsGetValue2Function tlsGetValueFunction = !tlsRc && tlsGetValue2 ? tlsGetValue2 : &TlsGetValue;

#endif

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

CThreadLocalBase::CThreadLocalBase()
{
#ifdef _WIN32
	m_index = TlsAlloc();
	AssertMsg( m_index != TLS_OUT_OF_INDEXES, "Bad thread local" );
	if ( m_index == TLS_OUT_OF_INDEXES )
		Error( "Out of thread local storage: %s\n",
			std::system_category().message(::GetLastError()).c_str() );
#elif defined(POSIX)
	if ( pthread_key_create( &m_index, NULL ) != 0 )
		Error( "Out of thread local storage!\n" );
#endif
}

//---------------------------------------------------------

CThreadLocalBase::~CThreadLocalBase()
{
#ifdef _WIN32
	if ( m_index != TLS_OUT_OF_INDEXES )
		TlsFree( m_index );
	m_index = TLS_OUT_OF_INDEXES;
#elif defined(POSIX)
	pthread_key_delete( m_index );
#endif
}

//---------------------------------------------------------

void * CThreadLocalBase::Get() const
{
#ifdef _WIN32
	// dimhotepus: Speedup access to thread local storage on Windows 11 24H2+.
	if ( m_index != TLS_OUT_OF_INDEXES )
		return tlsGetValueFunction( m_index );
	AssertMsg( 0, "Bad thread local" );
	return nullptr;
#elif defined(POSIX)
	void *value = pthread_getspecific( m_index );
	return value;
#endif
}

//---------------------------------------------------------

void CThreadLocalBase::Set( void *value )
{
#ifdef _WIN32
	if ( m_index != TLS_OUT_OF_INDEXES )
		TlsSetValue( m_index, value );
	else
		AssertMsg( 0, "Bad thread local" );
#elif defined(POSIX)
	if ( pthread_setspecific( m_index, value ) != 0 )
		AssertMsg( 0, "Bad thread local" );
#endif
}

//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------

#ifdef _WIN32
#ifdef _X360
#define TO_INTERLOCK_PARAM(p)		((long *)p)
#define TO_INTERLOCK_PTR_PARAM(p)	((void **)p)
#else
#define TO_INTERLOCK_PARAM(p)		(p)
#define TO_INTERLOCK_PTR_PARAM(p)	(p)
#endif

#ifndef USE_INTRINSIC_INTERLOCKED
long ThreadInterlockedIncrement( long volatile *pDest )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedIncrement( TO_INTERLOCK_PARAM(pDest) );
}

long ThreadInterlockedDecrement( long volatile *pDest )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedDecrement( TO_INTERLOCK_PARAM(pDest) );
}

long ThreadInterlockedExchange( long volatile *pDest, long value )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedExchange( TO_INTERLOCK_PARAM(pDest), value );
}

long ThreadInterlockedExchangeAdd( long volatile *pDest, long value )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedExchangeAdd( TO_INTERLOCK_PARAM(pDest), value );
}

long ThreadInterlockedCompareExchange( long volatile *pDest, long value, long comperand )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedCompareExchange( TO_INTERLOCK_PARAM(pDest), value, comperand );
}

bool ThreadInterlockedAssignIf( long volatile *pDest, long value, long comperand )
{
	Assert( (size_t)pDest % 4 == 0 );

#if !(defined(_WIN64) || defined (_X360))
	__asm 
	{
		mov	eax,comperand
		mov	ecx,pDest
		mov edx,value
		lock cmpxchg [ecx],edx 
		mov eax,0
		setz al
	}
#else
	return ( InterlockedCompareExchange( TO_INTERLOCK_PARAM(pDest), value, comperand ) == comperand );
#endif
}

#endif

#if !defined( USE_INTRINSIC_INTERLOCKED ) || defined( _WIN64 )
void *ThreadInterlockedExchangePointer( void * volatile *pDest, void *value )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedExchangePointer( TO_INTERLOCK_PARAM(pDest), value );
}

void *ThreadInterlockedCompareExchangePointer( void * volatile *pDest, void *value, void *comperand )
{
	Assert( (size_t)pDest % 4 == 0 );
	return InterlockedCompareExchangePointer( TO_INTERLOCK_PTR_PARAM(pDest), value, comperand );
}

bool ThreadInterlockedAssignPointerIf( void * volatile *pDest, void *value, void *comperand )
{
	Assert( (size_t)pDest % 4 == 0 );
#if !(defined(_WIN64) || defined (_X360))
	__asm 
	{
		mov	eax,comperand
		mov	ecx,pDest
		mov edx,value
		lock cmpxchg [ecx],edx 
		mov eax,0
		setz al
	}
#else
	return ( InterlockedCompareExchangePointer( TO_INTERLOCK_PTR_PARAM(pDest), value, comperand ) == comperand );
#endif
}
#endif

int64 ThreadInterlockedCompareExchange64( int64 volatile *pDest, int64 value, int64 comperand )
{
	Assert( (size_t)pDest % 8 == 0 );

#if defined(_WIN32)
	// Both x86 and x86-64.
	return InterlockedCompareExchange64( pDest, value, comperand );
#else
	__asm 
	{
		lea esi,comperand;
		lea edi,value;

		mov eax,[esi];
		mov edx,4[esi];
		mov ebx,[edi];
		mov ecx,4[edi];
		mov esi,pDest;
		lock cmpxchg8b [esi];			
	}
#endif
}

bool ThreadInterlockedAssignIf64(volatile int64 *pDest, int64 value, int64 comperand ) 
{
	Assert( (size_t)pDest % 8 == 0 );

	return ( ThreadInterlockedCompareExchange64( pDest, value, comperand ) == comperand ); 
}

#if defined( PLATFORM_64BITS )

bool ThreadInterlockedAssignIf128( volatile int128 *pDest, const int128 &value, const int128 &comperand )
{
	Assert( ( (size_t)pDest % 16 ) == 0 );

	volatile auto *pDest64 = ( volatile int64 * )pDest;
	auto *pValue64 = ( int64 * )&value;
	auto *pComperand64 = ( int64 * )&comperand;

	// Description:
	//  The CMPXCHG16B instruction compares the 128-bit value in the RDX:RAX and RCX:RBX registers
	//  with a 128-bit memory location. If the values are equal, the zero flag (ZF) is set,
	//  and the RCX:RBX value is copied to the memory location.
	//  Otherwise, the ZF flag is cleared, and the memory value is copied to RDX:RAX.

	// _InterlockedCompareExchange128: https://docs.microsoft.com/en-us/cpp/intrinsics/interlockedcompareexchange128
	return _InterlockedCompareExchange128( pDest64, pValue64[1], pValue64[0], pComperand64 ) == 1;
}

#endif // PLATFORM_64BITS

int64 ThreadInterlockedIncrement64( int64 volatile *pDest )
{
	Assert( (size_t)pDest % 8 == 0 );

	int64 Old;

	do 
	{
		Old = *pDest;
	} while (ThreadInterlockedCompareExchange64(pDest, Old + 1, Old) != Old);

	return Old + 1;
}

int64 ThreadInterlockedDecrement64( int64 volatile *pDest )
{
	Assert( (size_t)pDest % 8 == 0 );
	int64 Old;

	do 
	{
		Old = *pDest;
	} while (ThreadInterlockedCompareExchange64(pDest, Old - 1, Old) != Old);

	return Old - 1;
}

int64 ThreadInterlockedExchange64( int64 volatile *pDest, int64 value )
{
	Assert( (size_t)pDest % 8 == 0 );
	int64 Old;

	do 
	{
		Old = *pDest;
	} while (ThreadInterlockedCompareExchange64(pDest, value, Old) != Old);

	return Old;
}

int64 ThreadInterlockedExchangeAdd64( int64 volatile *pDest, int64 value )
{
	Assert( (size_t)pDest % 8 == 0 );
	int64 Old;

	do 
	{
		Old = *pDest;
	} while (ThreadInterlockedCompareExchange64(pDest, Old + value, Old) != Old);

	return Old;
}

#elif defined(GNUC)

#ifdef OSX
#include <libkern/OSAtomic.h>
#endif


long ThreadInterlockedIncrement( long volatile *pDest )
{
	return __sync_fetch_and_add( pDest, 1 ) + 1;
}

long ThreadInterlockedDecrement( long volatile *pDest )
{
	return __sync_fetch_and_sub( pDest, 1 ) - 1;
}

long ThreadInterlockedExchange( long volatile *pDest, long value )
{
	return __sync_lock_test_and_set( pDest, value );
}

long ThreadInterlockedExchangeAdd( long volatile *pDest, long value )
{
	return  __sync_fetch_and_add( pDest, value );
}

long ThreadInterlockedCompareExchange( long volatile *pDest, long value, long comperand )
{
	return  __sync_val_compare_and_swap( pDest, comperand, value );
}

bool ThreadInterlockedAssignIf( long volatile *pDest, long value, long comperand )
{
	return __sync_bool_compare_and_swap( pDest, comperand, value );
}

void *ThreadInterlockedExchangePointer( void * volatile *pDest, void *value )
{
	return __sync_lock_test_and_set( pDest, value );
}

void *ThreadInterlockedCompareExchangePointer( void *volatile *pDest, void *value, void *comperand )
{	
	return  __sync_val_compare_and_swap( pDest, comperand, value );
}

bool ThreadInterlockedAssignPointerIf( void * volatile *pDest, void *value, void *comperand )
{
	return  __sync_bool_compare_and_swap( pDest, comperand, value );
}

int64 ThreadInterlockedCompareExchange64( int64 volatile *pDest, int64 value, int64 comperand )
{
#if defined(OSX)
	int64 retVal = *pDest;
	if ( OSAtomicCompareAndSwap64( comperand, value, pDest ) )
		retVal = *pDest;
	
	return retVal;
#else
	return __sync_val_compare_and_swap( pDest, comperand, value  );
#endif
}

bool ThreadInterlockedAssignIf64( int64 volatile * pDest, int64 value, int64 comperand ) 
{
	return __sync_bool_compare_and_swap( pDest, comperand, value );
}

int64 ThreadInterlockedExchange64( int64 volatile *pDest, int64 value )
{
	Assert( (size_t)pDest % 8 == 0 );
	int64 Old;
	
	do 
	{
		Old = *pDest;
	} while (ThreadInterlockedCompareExchange64(pDest, value, Old) != Old);
	
	return Old;
}


#else
// This will perform horribly,
#error "Falling back to mutexed interlocked operations, you really don't have intrinsics you can use?"ß
CThreadMutex g_InterlockedMutex;

long ThreadInterlockedIncrement( long volatile *pDest )
{
	AUTO_LOCK( g_InterlockedMutex );
	return ++(*pDest);
}

long ThreadInterlockedDecrement( long volatile *pDest )
{
	AUTO_LOCK( g_InterlockedMutex );
	return --(*pDest);
}

long ThreadInterlockedExchange( long volatile *pDest, long value )
{
	AUTO_LOCK( g_InterlockedMutex );
	long retVal = *pDest;
	*pDest = value;
	return retVal;
}

void *ThreadInterlockedExchangePointer( void * volatile *pDest, void *value )
{
	AUTO_LOCK( g_InterlockedMutex );
	void *retVal = *pDest;
	*pDest = value;
	return retVal;
}

long ThreadInterlockedExchangeAdd( long volatile *pDest, long value )
{
	AUTO_LOCK( g_InterlockedMutex );
	long retVal = *pDest;
	*pDest += value;
	return retVal;
}

long ThreadInterlockedCompareExchange( long volatile *pDest, long value, long comperand )
{
	AUTO_LOCK( g_InterlockedMutex );
	long retVal = *pDest;
	if ( *pDest == comperand )
		*pDest = value;
	return retVal;
}

void *ThreadInterlockedCompareExchangePointer( void * volatile *pDest, void *value, void *comperand )
{
	AUTO_LOCK( g_InterlockedMutex );
	void *retVal = *pDest;
	if ( *pDest == comperand )
		*pDest = value;
	return retVal;
}


int64 ThreadInterlockedCompareExchange64( int64 volatile *pDest, int64 value, int64 comperand )
{
	Assert( (size_t)pDest % 8 == 0 );
	AUTO_LOCK( g_InterlockedMutex );
	int64 retVal = *pDest;
	if ( *pDest == comperand )
		*pDest = value;
	return retVal;
}

int64 ThreadInterlockedExchange64( int64 volatile *pDest, int64 value )
{
	Assert( (size_t)pDest % 8 == 0 );
	int64 Old;

	do 
	{
		Old = *pDest;
	} while (ThreadInterlockedCompareExchange64(pDest, value, Old) != Old);

	return Old;
}

bool ThreadInterlockedAssignIf64(volatile int64 *pDest, int64 value, int64 comperand ) 
{
	Assert( (size_t)pDest % 8 == 0 );
	return ( ThreadInterlockedCompareExchange64( pDest, value, comperand ) == comperand ); 
}

bool ThreadInterlockedAssignIf( long volatile *pDest, long value, long comperand )
{
	Assert( (size_t)pDest % 4 == 0 );
	return ( ThreadInterlockedCompareExchange( pDest, value, comperand ) == comperand ); 
}

#endif

//-----------------------------------------------------------------------------

#if defined(_WIN32) && defined(THREAD_PROFILER)
void ThreadNotifySyncNoop(void *p) {}

#define MAP_THREAD_PROFILER_CALL( from, to ) \
	void from(void *p) \
	{ \
		static CDynamicFunction<void (*)(void *)> dynFunc( "libittnotify.dll", #to, ThreadNotifySyncNoop ); \
		(*dynFunc)(p); \
	}

MAP_THREAD_PROFILER_CALL( ThreadNotifySyncPrepare, __itt_notify_sync_prepare );
MAP_THREAD_PROFILER_CALL( ThreadNotifySyncCancel, __itt_notify_sync_cancel );
MAP_THREAD_PROFILER_CALL( ThreadNotifySyncAcquired, __itt_notify_sync_acquired );
MAP_THREAD_PROFILER_CALL( ThreadNotifySyncReleasing, __itt_notify_sync_releasing );

#endif

//-----------------------------------------------------------------------------
//
// CThreadMutex
//
//-----------------------------------------------------------------------------

#ifndef POSIX
CThreadMutex::CThreadMutex() : CThreadMutex{4000}
{
}

// dimhotepus: Ctor with spin count.
CThreadMutex::CThreadMutex(unsigned int spinCount)
{
#ifdef THREAD_MUTEX_TRACING_ENABLED
	memset( &m_CriticalSection, 0, sizeof(m_CriticalSection) );
#endif
	static_assert(sizeof(m_CriticalSection) == sizeof(CRITICAL_SECTION));
	// This function always succeeds and returns a nonzero value on XP+.
	// See https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initializecriticalsectionandspincount
	(void)InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION *)&m_CriticalSection, spinCount);
#ifdef THREAD_MUTEX_TRACING_SUPPORTED
	// These need to be initialized unconditionally in case mixing release & debug object modules
	// Lock and unlock may be emitted as COMDATs, in which case may get spurious output
	m_currentOwnerID = m_lockCount = 0;
	m_bTrace = false;
#endif
}

CThreadMutex::~CThreadMutex()
{
	DeleteCriticalSection((CRITICAL_SECTION *)&m_CriticalSection);
}
#endif // !POSIX

bool CThreadMutex::TryLock()
{
#if defined( _WIN32 )
#ifdef THREAD_MUTEX_TRACING_ENABLED
	ThreadId_t thisThreadID = ThreadGetCurrentId();
	if ( m_bTrace && m_currentOwnerID && ( m_currentOwnerID != thisThreadID ) )
		Msg( "Thread %lu about to try-wait for lock %p owned by %lu\n", ThreadGetCurrentId(), (CRITICAL_SECTION *)&m_CriticalSection, m_currentOwnerID );
#endif

	// dimhotepus: Use TryEnterCriticalSection directly instead of dynamic linking.
	if ( TryEnterCriticalSection( (CRITICAL_SECTION *)&m_CriticalSection ) != FALSE )
	{
#ifdef THREAD_MUTEX_TRACING_ENABLED
		if (m_lockCount == 0)
		{
			// we now own it for the first time.  Set owner information
			m_currentOwnerID = thisThreadID;
			if ( m_bTrace )
				Msg( "Thread %lu now owns lock 0x%p\n", m_currentOwnerID, (CRITICAL_SECTION *)&m_CriticalSection );
		}
		m_lockCount++;
#endif
		return true;
	}

	return false;
#elif defined( POSIX )
	return pthread_mutex_trylock( &m_Mutex ) == 0;
#else
#error "Implement me!"
	return true;
#endif
}

//-----------------------------------------------------------------------------
//
// CThreadFastMutex
//
//-----------------------------------------------------------------------------

enum {
  THREAD_SPIN = (8*1024)
};

void CThreadFastMutex::Lock( const ThreadId_t threadId, unsigned nSpinSleepTime ) 
{
	int i;
	if ( nSpinSleepTime != TT_INFINITE )
	{
		for ( i = THREAD_SPIN; i != 0; --i )
		{
			if ( TryLock( threadId ) )
			{
				return;
			}
			ThreadPause();
		}

		for ( i = THREAD_SPIN; i != 0; --i )
		{
			if ( TryLock( threadId ) )
			{
				return;
			}
			ThreadPause();
			if ( i % 1024 == 0 )
			{
				ThreadSleep( 0 );
			}
		}

#ifdef _WIN32
		if ( !nSpinSleepTime && GetThreadPriority( GetCurrentThread() ) > THREAD_PRIORITY_NORMAL )
		{
			nSpinSleepTime = 1;
		} 
		else
#endif

		if ( nSpinSleepTime )
		{
			for ( i = THREAD_SPIN; i != 0; --i )
			{
				if ( TryLock( threadId ) )
				{
					return;
				}

				ThreadPause();
				ThreadSleep( 0 );
			}

		}

		for ( ;; ) // coded as for instead of while to make easy to breakpoint success
		{
			if ( TryLock( threadId ) )
			{
				return;
			}

			ThreadPause();
			ThreadSleep( nSpinSleepTime );
		}
	}
	else
	{
		for ( ;; ) // coded as for instead of while to make easy to breakpoint success
		{
			if ( TryLock( threadId ) )
			{
				return;
			}

			ThreadPause();
		}
	}
}

//-----------------------------------------------------------------------------
//
// CThreadRWLock
//
//-----------------------------------------------------------------------------

void CThreadRWLock::WaitForRead()
{
	m_nPendingReaders++;

	do
	{
		m_mutex.Unlock();
		m_CanRead.Wait();
		m_mutex.Lock();
	}
	while (m_nWriters);

	m_nPendingReaders--;
}


void CThreadRWLock::LockForWrite()
{
	bool bWait;

	{
		AUTO_LOCK(m_mutex);

		bWait = m_nWriters != 0 || m_nActiveReaders != 0;

		++m_nWriters;
		m_CanRead.Reset();
	}

	if ( bWait )
	{
		m_CanWrite.Wait();
	}
}

void CThreadRWLock::UnlockWrite()
{
	AUTO_LOCK(m_mutex);
	if ( --m_nWriters == 0)
	{
		if ( m_nPendingReaders )
		{
			m_CanRead.Set();
		}
	}
	else
	{
		m_CanWrite.Set();
	}
}

//-----------------------------------------------------------------------------
//
// CThreadSpinRWLock
//
//-----------------------------------------------------------------------------

void CThreadSpinRWLock::SpinLockForWrite( const ThreadId_t threadId )
{
	for ( int i = 1000; i != 0; --i )
	{
		if ( TryLockForWrite( threadId ) )
		{
			return;
		}
		ThreadPause();
	}

	for ( int i = 20000; i != 0; --i )
	{
		if ( TryLockForWrite( threadId ) )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 0 );
	}

	for ( ;; ) // coded as for instead of while to make easy to breakpoint success
	{
		if ( TryLockForWrite( threadId ) )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 1 );
	}
}

void CThreadSpinRWLock::LockForRead()
{
	for ( int i = 1000; i != 0; --i )
	{
		if ( TryLockForRead() )
		{
			return;
		}
		ThreadPause();
	}

	for ( int i = 20000; i != 0; --i )
	{
		if ( TryLockForRead() )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 0 );
	}

	for ( ;; ) // coded as for instead of while to make easy to breakpoint success
	{
		if ( TryLockForRead() )
		{
			return;
		}

		ThreadPause();
		ThreadSleep( 1 );
	}
}

void CThreadSpinRWLock::UnlockRead()
{
	const auto lockInfo = m_lockInfo.load();
	Assert( lockInfo.m_nReaders > 0 && lockInfo.m_writerId == 0 );

	LockInfo_t oldValue{0UL, lockInfo.m_nReaders};
	LockInfo_t newValue{0UL, oldValue.m_nReaders - 1};
	
	if( AssignIf( newValue, oldValue ) )
		return;
	ThreadPause();
	oldValue.m_nReaders = m_lockInfo.load().m_nReaders;
	newValue.m_nReaders = oldValue.m_nReaders - 1;

	for ( int i = 500; i != 0; --i )
	{
		if( AssignIf( newValue, oldValue ) )
			return;
		ThreadPause();
		oldValue.m_nReaders = m_lockInfo.load().m_nReaders;
		newValue.m_nReaders = oldValue.m_nReaders - 1;
	}

	for ( int i = 20000; i != 0; --i )
	{
		if( AssignIf( newValue, oldValue ) )
			return;
		ThreadPause();
		ThreadSleep( 0 );
		oldValue.m_nReaders = m_lockInfo.load().m_nReaders;
		newValue.m_nReaders = oldValue.m_nReaders - 1;
	}

	for ( ;; ) // coded as for instead of while to make easy to breakpoint success
	{
		if( AssignIf( newValue, oldValue ) )
			return;
		ThreadPause();
		ThreadSleep( 1 );
		oldValue.m_nReaders = m_lockInfo.load().m_nReaders;
		newValue.m_nReaders = oldValue.m_nReaders - 1;
	}
}

void CThreadSpinRWLock::UnlockWrite()
{
	const auto lockInfo = m_lockInfo.load();

	Assert( lockInfo.m_writerId == ThreadGetCurrentId() );
	Assert( lockInfo.m_nReaders == 0 );

	m_lockInfo.exchange(LockInfo_t{0, 0});
	--m_nWriters;
}



//-----------------------------------------------------------------------------
//
// CThread
//
//-----------------------------------------------------------------------------

static thread_local CThread *g_pCurThread;

//---------------------------------------------------------

CThread::CThread()
:	
#ifdef _WIN32
	m_hThread( nullptr ),
#endif
	m_threadId( 0 ),
	m_result( 0 ),
	m_pStackBase{ nullptr },
	m_flags( 0 )
{
	m_szName[0] = '\0';
}

//---------------------------------------------------------

CThread::~CThread()
{
#ifdef _WIN32
	if (m_hThread)
#elif defined(POSIX)
	if ( m_threadId )
#endif
	{
		if ( IsAlive() )
		{
			Msg( "Illegal termination of worker thread! Threads must negotiate an end to the thread before the CThread object is destroyed.\n" ); 
#ifdef _WIN32

			if ( DoNewAssertDialog( __FILE__, __LINE__,
				"Illegal termination of worker thread! Threads must negotiate an end to the thread before the CThread object is destroyed.\n" ) )
			{
				// dimhotepus: If user want to break debugger then do it.
				DebuggerBreakIfDebugging();
			}
#endif
			if ( GetCurrentCThread() == this )
			{
				Stop(); // BUGBUG: Alfred - this doesn't make sense, this destructor fires from the hosting thread not the thread itself!!
			}
		}

#ifdef _WIN32
		// Now that the worker thread has exited (which we know because we presumably waited
		// on the thread handle for it to exit) we can finally close the thread handle. We
		// cannot do this any earlier, and certainly not in CThread::ThreadProc().
		CloseHandle( m_hThread );
#endif
	}
}


//---------------------------------------------------------

const char *CThread::GetName()
{
	AUTO_LOCK( m_Lock );
	if ( !m_szName[0] )
	{
#ifdef _WIN32
		_snprintf( m_szName, std::size(m_szName) - 1, "Thread(%p/%p)", this, m_hThread );
#elif defined(POSIX)
		_snprintf( m_szName, std::size(m_szName) - 1, "Thread(%p/0x%x)", this, (uint)m_threadId );
#endif
		m_szName[std::size(m_szName) - 1] = '\0';
	}
	return m_szName;
}

//---------------------------------------------------------

void CThread::SetName(const char *pszName)
{
	AUTO_LOCK( m_Lock );
	strncpy( m_szName, pszName, std::size(m_szName) );
	m_szName[std::size(m_szName) - 1] = '\0';
}

//---------------------------------------------------------

bool CThread::Start( unsigned nBytesStack )
{
	AUTO_LOCK( m_Lock );

	if ( IsAlive() )
	{
		AssertMsg( 0, "Tried to create a thread that has already been created!" );
		return false;
	}

	bool  bInitSuccess = false;
	CThreadEvent createComplete;
	ThreadInit_t init = { this, &createComplete, &bInitSuccess };

#ifdef _WIN32
	HANDLE       hThread;
	m_hThread = hThread = (HANDLE)VCRHook_CreateThread( nullptr,
														nBytesStack,
														reinterpret_cast<void*>(GetThreadProc()),
														new ThreadInit_t(init),
														CREATE_SUSPENDED,
														&m_threadId );
	if ( !hThread )
	{
		AssertMsg1( 0, "Failed to create thread: %s",
			std::system_category().message(::GetLastError()).c_str() );
		return false;
	}
	Plat_ApplyHardwareDataBreakpointsToNewThread( m_threadId );
	ResumeThread( hThread );

#elif defined(POSIX)
	pthread_attr_t attr;
	pthread_attr_init( &attr );
	// From https://man7.org/linux/man-pages/man3/pthread_attr_setstacksize.3.html
	//  A thread's stack size is fixed at the time of thread creation. Only the main thread can dynamically grow its stack.
	pthread_attr_setstacksize( &attr, MAX( nBytesStack, 1024u*1024 ) );
	if ( pthread_create( &m_threadId, &attr, (void *(*)(void *))GetThreadProc(), new ThreadInit_t( init ) ) != 0 )
	{
		AssertMsg1( 0, "Failed to create thread: %s",
			std::system_category().message(errno).c_str() );
		return false;
	}
	Plat_ApplyHardwareDataBreakpointsToNewThread( (long unsigned int)m_threadId );
	bInitSuccess = true;
#endif

#if !defined( OSX )
	ThreadSetDebugName( m_threadId, m_szName );
#endif

	if ( !WaitForCreateComplete( &createComplete ) )
	{
		Msg( "Thread failed to initialize\n" );
#ifdef _WIN32
		CloseHandle( m_hThread );
		m_hThread = nullptr;
		m_threadId = 0;
#elif defined(POSIX)
		m_threadId = 0;
#endif
		return false;
	}

	if ( !bInitSuccess )
	{
		Msg( "Thread failed to initialize\n" );
#ifdef _WIN32
		CloseHandle( m_hThread );
		m_hThread = nullptr;
		m_threadId = 0;
#elif defined(POSIX)
		m_threadId = 0;
#endif
		return false;
	}

#ifdef _WIN32
	if ( !m_hThread )
	{
		Msg( "Thread exited immediately\n" );
	}
#endif

#ifdef _WIN32
	return !!m_hThread;
#elif defined(POSIX)
	return !!m_threadId;
#endif
}

//---------------------------------------------------------
//
// Return true if the thread exists. false otherwise
//

bool CThread::IsAlive() const
{
#ifdef _WIN32
	DWORD dwExitCode;

	return ( m_hThread &&
		GetExitCodeThread( m_hThread, &dwExitCode ) &&
		dwExitCode == STILL_ACTIVE );
#elif defined(POSIX)
	return m_threadId;
#endif
}

//---------------------------------------------------------

bool CThread::Join([[maybe_unused]] unsigned timeout)
{
#ifdef _WIN32
	if ( m_hThread )
#elif defined(POSIX)
	if ( m_threadId )
#endif
	{
		AssertMsg(GetCurrentCThread() != this, _T("Thread cannot be joined with self"));

#ifdef _WIN32
		return ThreadJoin( (ThreadHandle_t)m_hThread );
#elif defined(POSIX)
		return ThreadJoin( (ThreadHandle_t)m_threadId );
#endif
	}
	return true;
}

//---------------------------------------------------------

#ifdef _WIN32

HANDLE CThread::GetThreadHandle()
{
	return m_hThread;
}

#endif

#if defined( _WIN32 ) || defined( LINUX )

//---------------------------------------------------------

uint CThread::GetThreadId()
{
	return m_threadId;
}

#endif

//---------------------------------------------------------

int CThread::GetResult() const
{
	return m_result;
}

//---------------------------------------------------------
//
// Forcibly, abnormally, but relatively cleanly stop the thread
//

void CThread::Stop(int exitCode)
{
	if ( !IsAlive() )
		return;

	if ( GetCurrentCThread() == this )
	{
		m_result = exitCode;
		if ( !( m_flags & SUPPORT_STOP_PROTOCOL ) )
		{
			OnExit();
			g_pCurThread = nullptr;

#ifdef _WIN32
			CloseHandle( m_hThread );
			m_hThread = nullptr;
#endif
			Cleanup();
		}
		throw exitCode;
	}
	else
		AssertMsg( 0, "Only thread can stop self: Use a higher-level protocol");
}

//---------------------------------------------------------

int CThread::GetPriority() const
{
#ifdef _WIN32
	return GetThreadPriority(m_hThread);
#elif defined(POSIX)
	struct sched_param thread_param;
	int policy;
	pthread_getschedparam( m_threadId, &policy, &thread_param );
	return thread_param.sched_priority;
#endif
}

//---------------------------------------------------------

bool CThread::SetPriority(int priority)
{
#ifdef _WIN32
	return ThreadSetPriority( (ThreadHandle_t)m_hThread, priority );
#else
	return ThreadSetPriority( (ThreadHandle_t)m_threadId, priority );
#endif
}


//---------------------------------------------------------

void CThread::SuspendCooperative()
{
	if ( ThreadGetCurrentId() == m_threadId )
	{
		m_SuspendEventSignal.Set();
		m_nSuspendCount = 1;
		m_SuspendEvent.Wait();
		m_nSuspendCount = 0;
	}
	else
	{
		AssertMsg( false, "Suspend not called from worker thread, this would be a bug" );
	}
}

//---------------------------------------------------------

void CThread::ResumeCooperative()
{
	// TODO: dimhotepus: Sometimes Assert fires, investigate.
	[[maybe_unused]] int suspendCount = m_nSuspendCount;
	AssertMsg( suspendCount == 1, "Suspend event count %d should be 1", suspendCount );
	m_SuspendEvent.Set();
}


void CThread::BWaitForThreadSuspendCooperative()
{
	m_SuspendEventSignal.Wait();
}


#ifndef LINUX
//---------------------------------------------------------

unsigned int CThread::Suspend()
{
#ifdef _WIN32
  // dimhotepus: x64 support.
#ifndef PLATFORM_64BITS
  return SuspendThread(m_hThread) != static_cast<DWORD>(-1); //-V720
#else
  return Wow64SuspendThread(m_hThread) != static_cast<DWORD>(-1);
#endif
#elif defined(OSX)
	int susCount = m_nSuspendCount++;
	while ( thread_suspend( pthread_mach_thread_np(m_threadId) ) != KERN_SUCCESS )
	{
	};
	return ( susCount) != 0;
#else
#error "Please define your platform"
#endif
}

//---------------------------------------------------------

unsigned int CThread::Resume()
{
#ifdef _WIN32
	return ( ResumeThread(m_hThread) != 0 );
#elif defined(OSX)
	int susCount = m_nSuspendCount++;
	while ( thread_resume( pthread_mach_thread_np(m_threadId) )  != KERN_SUCCESS )
	{
	};	
	return ( susCount - 1) != 0;
#else
#error "Please define your platform"
#endif
}
#endif


//---------------------------------------------------------

bool CThread::Terminate(int exitCode)
{
#ifndef _X360
#ifdef _WIN32
	// I hope you know what you're doing!
	if (!TerminateThread(m_hThread, exitCode))
		return false;
	CloseHandle( m_hThread );
	m_hThread = nullptr;
	Cleanup();
#elif defined(POSIX)
	pthread_kill( m_threadId, SIGKILL );
	Cleanup();
#endif

	return true;
#else
	AssertMsg( 0, "Cannot terminate a thread on the Xbox!" );
	return false;
#endif
}

//---------------------------------------------------------
//
// Get the Thread object that represents the current thread, if any.
// Can return NULL if the current thread was not created using
// CThread
//

CThread *CThread::GetCurrentCThread()
{
	return g_pCurThread;
}

//---------------------------------------------------------
//
// Offer a context switch.
//

void CThread::Yield()
{
#ifdef _WIN32
	SwitchToThread();
#elif defined(POSIX)
	pthread_yield();
#endif
}

//---------------------------------------------------------
//
// This method causes the current thread to yield and not to be
// scheduled for further execution until a certain amount of real
// time has elapsed, more or less.
//

void CThread::Sleep(unsigned duration)
{
	::ThreadSleep(duration);
}

//---------------------------------------------------------

bool CThread::Init()
{
	return true;
}

//---------------------------------------------------------

void CThread::OnExit()
{
}

//---------------------------------------------------------

void CThread::Cleanup()
{
	m_threadId = 0;
}

//---------------------------------------------------------
bool CThread::WaitForCreateComplete(CThreadEvent * pEvent)
{
	// Force serialized thread creation...
	if (!pEvent->Wait(60000))
	{
		AssertMsg( 0, "Probably deadlock or failure waiting for thread to initialize." );
		return false;
	}
	return true;
}

//---------------------------------------------------------

bool CThread::IsThreadRunning()
{
#ifdef _PS3
    // ThreadIsThreadIdRunning() doesn't work on PS3 if the thread is in a zombie state
    return m_eventTheadExit.Check();
#else
    return ThreadIsThreadIdRunning( (ThreadId_t)m_threadId );
#endif
}

//---------------------------------------------------------

CThread::ThreadProc_t CThread::GetThreadProc()
{
	return ThreadProc;
}

//---------------------------------------------------------

unsigned __stdcall CThread::ThreadProc(LPVOID pv)
{
	// dimhotepus: std::auto_ptr -> std::unique_ptr
	std::unique_ptr<ThreadInit_t> pInit((ThreadInit_t *)pv);
 
	CThread *pThread = pInit->pThread;
	g_pCurThread = pThread;
	
	pThread->m_pStackBase = AlignValue(&pThread, 4096);
	pThread->m_result = -1;
	
	bool bInitSuccess = true;
	if ( pInit->pfInitSuccess )
		*(pInit->pfInitSuccess) = false;
	
	try
	{
		bInitSuccess = pThread->Init();
	}
	catch (...)
	{
		pInit->pInitCompleteEvent->Set();
		throw;
	}
	
	if ( pInit->pfInitSuccess )
		*(pInit->pfInitSuccess) = bInitSuccess;
	pInit->pInitCompleteEvent->Set();
	if (!bInitSuccess)
		return 0;
	
	if ( pThread->m_flags & SUPPORT_STOP_PROTOCOL )
	{
		try
		{
			pThread->m_result = pThread->Run();
		}
		catch ( const std::exception &e )
		{
			Warning( "Thread exception occured: %s", e.what() );
		}
	}
	else
	{
		pThread->m_result = pThread->Run();
	}
	
	pThread->OnExit();
	g_pCurThread = nullptr;
	pThread->Cleanup();
	
	return pThread->m_result;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CWorkerThread::CWorkerThread()
:	m_EventSend(true),                 // must be manual-reset for PeekCall()
	m_EventComplete(true),             // must be manual-reset to handle multiple wait with thread properly
	m_Param(0),
	m_pParamFunctor(nullptr),
	m_ReturnVal(0)
{
}

//---------------------------------------------------------

int CWorkerThread::CallWorker(unsigned dw, unsigned timeout, bool fBoostWorkerPriorityToMaster, CFunctor *pParamFunctor)
{
	return Call(dw, timeout, fBoostWorkerPriorityToMaster, nullptr, pParamFunctor);
}

//---------------------------------------------------------

int CWorkerThread::CallMaster(unsigned dw, unsigned timeout)
{
	return Call(dw, timeout, false);
}

//---------------------------------------------------------

CThreadEvent &CWorkerThread::GetCallHandle()
{
	return m_EventSend;
}

//---------------------------------------------------------

unsigned CWorkerThread::GetCallParam( CFunctor **ppParamFunctor ) const
{
	if( ppParamFunctor )
		*ppParamFunctor = m_pParamFunctor;
	return m_Param;
}

//---------------------------------------------------------

int CWorkerThread::BoostPriority()
{
	int iInitialPriority = GetPriority();
	const int iNewPriority = ThreadGetPriority( (ThreadHandle_t)GetThreadID() );
	if (iNewPriority > iInitialPriority)
		ThreadSetPriority( (ThreadHandle_t)GetThreadID(), iNewPriority);
	return iInitialPriority;
}

//---------------------------------------------------------

static uint32 __stdcall DefaultWaitFunc( int nEvents, CThreadEvent * const *pEvents, int bWaitAll, uint32 timeout )
{
	return ThreadWaitForEvents( nEvents, pEvents, bWaitAll!=0, timeout );
//	return VCRHook_WaitForMultipleObjects( nHandles, (const void **)pHandles, bWaitAll, timeout );
}


int CWorkerThread::Call(unsigned dwParam, unsigned timeout, bool fBoostPriority, WaitFunc_t pfnWait, CFunctor *pParamFunctor)
{
	AssertMsg(!m_EventSend.Check(), "Cannot perform call if there's an existing call pending" );

	AUTO_LOCK( m_Lock );

	if (!IsAlive())
		return WTCR_FAIL;

	int iInitialPriority = 0;
	if (fBoostPriority)
	{
		iInitialPriority = BoostPriority();
	}

	// set the parameter, signal the worker thread, wait for the completion to be signaled
	m_Param = dwParam;
	m_pParamFunctor = pParamFunctor;

	m_EventComplete.Reset();
	m_EventSend.Set();

	WaitForReply( timeout, pfnWait );

	// MWD: Investigate why setting thread priorities is killing the 360
#ifndef _X360
	if (fBoostPriority)
		SetPriority(iInitialPriority);
#endif

	return m_ReturnVal;
}

//---------------------------------------------------------
//
// Wait for a request from the client
//
//---------------------------------------------------------
int CWorkerThread::WaitForReply( unsigned timeout )
{
	return WaitForReply( timeout, nullptr );
}

int CWorkerThread::WaitForReply( unsigned timeout, WaitFunc_t pfnWait )
{
	if (!pfnWait)
	{
		pfnWait = DefaultWaitFunc;
	}

#ifdef WIN32
	CThreadEvent threadEvent( GetThreadHandle() );
#endif
	
	CThreadEvent *waits[] =
	{
#ifdef WIN32
		&threadEvent,
#endif
		&m_EventComplete
	};
	

	unsigned result;
	bool bInDebugger = Plat_IsInDebugSession();

	do
	{
#ifdef WIN32
		// Make sure the thread handle hasn't been closed
		if ( !GetThreadHandle() )
		{
			result = WAIT_OBJECT_0 + 1;
			break;
		}
#endif
		result = (*pfnWait)(std::size(waits), waits, 0,
			(timeout != TT_INFINITE) ? timeout : 30000);

		AssertMsg(timeout != TT_INFINITE || result != WAIT_TIMEOUT, "Possible hung thread, call to thread timed out");

	} while ( bInDebugger && ( timeout == TT_INFINITE && result == WAIT_TIMEOUT ) );

	if ( result != WAIT_OBJECT_0 + 1 )
	{
		if (result == WAIT_TIMEOUT)
			m_ReturnVal = WTCR_TIMEOUT;
		else if (result == WAIT_OBJECT_0)
		{
			DevMsg( 2, "Thread failed to respond, probably exited\n");
			m_EventSend.Reset();
			m_ReturnVal = WTCR_TIMEOUT;
		}
		else
		{
			m_EventSend.Reset();
			m_ReturnVal = WTCR_THREAD_GONE;
		}
	}

	return m_ReturnVal;
}


//---------------------------------------------------------
//
// Wait for a request from the client
//
//---------------------------------------------------------

bool CWorkerThread::WaitForCall(unsigned * pResult)
{
	return WaitForCall(TT_INFINITE, pResult);
}

//---------------------------------------------------------

bool CWorkerThread::WaitForCall(unsigned dwTimeout, unsigned * pResult)
{
	bool returnVal = m_EventSend.Wait(dwTimeout);
	if (pResult)
		*pResult = m_Param;
	return returnVal;
}

//---------------------------------------------------------
//
// is there a request?
//

bool CWorkerThread::PeekCall(unsigned * pParam, CFunctor **ppParamFunctor)
{
	if (!m_EventSend.Check())
	{
		return false;
	}
	else
	{
		if (pParam)
		{
			*pParam = m_Param;
		}
		if( ppParamFunctor )
		{
			*ppParamFunctor = m_pParamFunctor;
		}
		return true;
	}
}

//---------------------------------------------------------
//
// Reply to the request
//

void CWorkerThread::Reply(unsigned dw)
{
	m_Param = 0;
	m_ReturnVal = dw;

	// The request is now complete so PeekCall() should fail from
	// now on
	//
	// This event should be reset BEFORE we signal the client
	m_EventSend.Reset();

	// Tell the client we're finished
	m_EventComplete.Set();
}

//-----------------------------------------------------------------------------
