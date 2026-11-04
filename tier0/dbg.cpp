// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"
#include "tier0/minidump.h"

#if defined( _WIN32 ) && !defined( _X360 )
#include "tier0/valve_off.h"
#endif

// dimhotepus: Include what we use instead of relying on transitive includes.
#include <algorithm>     // std::lower_bound, std::min
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <climits>       // INT_MIN
#include <cmath>         // std::fabs
#include <csignal>       // SIGABRT
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>      // std::size
#include <mutex>
#include <shared_mutex>
#include <system_error>  // std::generic_category
#include <thread>
#include <tuple>         // std::tie
#include <utility>

#include "Color.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier0/icommandline.h"
#include "posix_file_stream.h"

#include "tier0/etwprof.h"

#ifndef STEAM
#define PvRealloc realloc
#define PvAlloc malloc
#define PvFree free
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// internal structures
//-----------------------------------------------------------------------------
enum 
{ 
	MAX_GROUP_NAME_LENGTH = 48 
};

struct SpewGroup_t
{
	tchar	m_GroupName[MAX_GROUP_NAME_LENGTH];
	int		m_Level;
};

// dimhotepus: Internal helpers moved into an anonymous namespace (internal linkage).
namespace
{

// dimhotepus: Storage for an object that is constructed on first use and never destroyed.
// Spew can be emitted during static initialization of other translation units
// (before our globals are constructed) and from other threads during static
// destruction (after they are destroyed); both are UB with plain globals.
// Uses no operator new, so it is unaffected by memdbgon.h.
template< typename T >
union NeverDestroyed
{
	template< typename... Args >
	explicit NeverDestroyed( Args &&...args ) :
		m_value( std::forward< Args >( args )... )
	{
	}
	~NeverDestroyed() {}

	NeverDestroyed( const NeverDestroyed & ) = delete;
	NeverDestroyed &operator=( const NeverDestroyed & ) = delete;

	T m_value;
};


// dimhotepus: static const -> static constexpr. Color has a constexpr ctor and a
// trivial destructor, so this is constant-initialized (no static init order
// fiasco) and never destroyed at exit; safe to use at any time from any thread.
constexpr Color s_DefaultOutputColor( 255, 255, 255, 255 );

// Skip forward past the directory
// dimhotepus: Returns const tchar* (was const char*) to match the parameter type.
[[nodiscard]] const tchar *SkipToFname( const tchar* pFile )
{
	if ( pFile == nullptr )
		return _T("unknown");

	const tchar* pSlash = _tcsrchr( pFile, _T('\\') );
	const tchar* pSlash2 = _tcsrchr( pFile, _T('/') );
	// dimhotepus: Relational comparison against nullptr is unspecified, handle explicitly.
	if ( !pSlash || ( pSlash2 && pSlash2 > pSlash ) )
		pSlash = pSlash2;

	return pSlash ? pSlash + 1 : pFile;
}

// dimhotepus: Is string uniform?
[[nodiscard]] inline bool V_isuniform(IN_Z const char* string, char value) {
  if (!string || !*string) return false;
  while (*string) {
    if (*string != value) {
      return false;
    }
    ++string;
  }
  return true;
}

// dimhotepus: Bounded vsnprintf with identical semantics everywhere.
// C99 vsnprintf returns the would-be length on truncation, MSVC _vsnprintf
// returns -1 and does not terminate. The old code added the raw return value
// to the write offset, so a long message overflowed the stack buffer on POSIX.
// Always terminates, returns the number of characters actually stored.
size_t SafeVFormat( tchar *out, size_t capacity, const tchar *format, va_list args )
{
	if ( !out || capacity == 0 )
		return 0;

	if ( !format )
	{
		out[0] = _T('\0');
		return 0;
	}

	const int rc = _vsntprintf( out, capacity, format, args );
	out[capacity - 1] = _T('\0');

	if ( rc < 0 )
	{
		// MSVC truncation or encoding error, keep whatever was written.
		return _tcslen( out );
	}

	return std::min( static_cast<size_t>( rc ), capacity - 1 );
}

// dimhotepus: Variadic wrapper over SafeVFormat.
size_t SafeFormat( tchar *out, size_t capacity, PRINTF_FORMAT_STRING const tchar *format, ... )
{
	va_list args;
	va_start( args, format ); //-V2018 //-V2019
	const size_t written = SafeVFormat( out, capacity, format, args );
	va_end( args );
	return written;
}


//-----------------------------------------------------------------------------
// dimhotepus: Process termination.
//
// exit() must not be called concurrently or more than once (UB: it runs atexit
// handlers and static destructors). Several threads may hit a fatal assert or
// SPEW_ABORT at the same time, so the first one wins and the rest park.
//-----------------------------------------------------------------------------
std::atomic_flag s_bProcessExiting = ATOMIC_FLAG_INIT;
thread_local bool t_bOwnsProcessExit = false;

void ClaimProcessExitOrPark()
{
	if ( t_bOwnsProcessExit )
		return;

	if ( !s_bProcessExiting.test_and_set( std::memory_order_acq_rel ) )
	{
		t_bOwnsProcessExit = true;
		return;
	}

	// Another thread is already terminating the process.
	for ( ;; )
	{
		std::this_thread::sleep_for( std::chrono::hours( 1 ) );
	}
}

[[noreturn]] void ExitProcessAsAbort()
{
	ClaimProcessExitOrPark();

	fflush( nullptr );

#ifdef _WIN32
	// dimhotepus: Abort on windows exits with error code 3.
	// See https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/abort
	exit( 3 );  //-V2014
#else
	// dimhotepus: Abort on POSIX is 128 + signal abort code.
	// See https://stackoverflow.com/questions/23098695/strange-return-value-134-to-call-gawk-in-a-bash-script
	exit( 128 + SIGABRT );  //-V2014
#endif
}

// dimhotepus: Thread-safe "warn only once per call site", replaces ExecuteOnce
// whose thread-safety is not guaranteed.
#define DBG_WARNING_ONCE( ... )												\
	do {																	\
		static std::atomic<bool> s_bWarned{ false };						\
		if ( !s_bWarned.exchange( true, std::memory_order_relaxed ) )		\
			Warning( __VA_ARGS__ );											\
	} while ( 0 )

} // namespace


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

// dimhotepus: Make all this things thread-safe as may be used from different threads.
// All atomics below are constant-initialized, so they are safe to use during
// static initialization of other translation units.
// dimhotepus: Brace initialization (constant initialization guaranteed).
static std::atomic<SpewOutputFunc_t>			s_SpewOutputFunc{ DefaultSpewFunc };
static std::atomic<AssertFailedNotifyFunc_t>	s_AssertFailedNotifyFunc{ nullptr };

// Per thread, set by _SpewInfo right before an assert spews.
static thread_local const tchar*	s_pFileName = nullptr;
static thread_local int				s_Line = 0;
static thread_local SpewType_t		s_SpewType = SPEW_MESSAGE;

// dimhotepus: CThreadMutex -> std::shared_mutex. Readers (every D*Msg call) take
// a shared lock, writers (SpewActivate / SpewDeactivate) an exclusive one, so
// concurrent spew no longer serializes. Constructed on first use, never
// destroyed (was a global with a dynamic initializer, init order fiasco).
static std::shared_mutex &SpewGroupsMutex()
{
	static NeverDestroyed<std::shared_mutex> s_mutex;
	return s_mutex.m_value;
}
static SpewGroup_t*			s_pSpewGroups = nullptr;	// guarded by SpewGroupsMutex(), sorted case-insensitively
static size_t				s_GroupCount = 0;			// guarded by SpewGroupsMutex()
static std::atomic_int		s_DefaultLevel{ 0 };

// Only useable from within a spew function
struct SpewInfo_t
{
	const Color*	m_pSpewOutputColor;
	const tchar*	m_pSpewOutputGroup;
	int				m_nSpewOutputLevel;
};

static thread_local SpewInfo_t *g_pSpewInfo = nullptr;

// dimhotepus: Spew functions may spew; bound that so a broken spew function (e.g. one that
// asserts on every call) cannot blow the stack.
static thread_local int t_nSpewDepth = 0;
constexpr int kMaxSpewDepth = 8;


// Standard groups
static constexpr tchar s_pDeveloper[] = _T("developer");
static constexpr tchar s_pConsole[] = _T("console");
static constexpr tchar s_pNetwork[] = _T("network");

enum StandardSpewGroup_t
{
	GROUP_DEVELOPER = 0,
	GROUP_CONSOLE,
	GROUP_NETWORK,

	GROUP_COUNT,
};

// dimhotepus: const char* -> const tchar *const (type matches the names, array is immutable).
// dimhotepus: s_pGroupIndices removed, replaced by s_StandardGroupLevels below.
static const tchar *const s_pGroupNames[GROUP_COUNT] = { s_pDeveloper, s_pConsole, s_pNetwork };

// dimhotepus: Lock-free cache of the standard group levels. DevMsg / ConMsg /
// NetMsg are by far the most frequent callers and no longer take any lock.
// Written under the exclusive group lock, so it always agrees with the table.
constexpr int kSpewLevelUnset = INT_MIN;
static std::atomic_int s_StandardGroupLevels[GROUP_COUNT] =
{
	kSpewLevelUnset,
	kSpewLevelUnset,
	kSpewLevelUnset
};


//-----------------------------------------------------------------------------
// Default spew function
//-----------------------------------------------------------------------------
// dimhotepus: static, internal linkage.
template <size_t out_size>
static const char *PrefixMessageGroup(OUT_Z_ARRAY char (&out)[out_size], const char *group,
                                      const char *message) {
  // dimhotepus: Read the thread-local directly: DefaultSpewFunc may be called
  // outside of a spew dispatch (chained from user code), where
  // GetSpewOutputGroup() asserts.
  const SpewInfo_t *info{g_pSpewInfo};
  const char *out_group{info ? info->m_pSpewOutputGroup : nullptr};

  out_group = out_group && out_group[0] ? out_group : group;

  // dimhotepus: snprintf always terminates, pass the full size (was size - 1).
  const size_t length{strlen(message)};
  if (length > 1 && message[length - 1] == '\n' && !V_isuniform(message, '\n')) {
    snprintf(out, out_size, "[%.3f][%s] %s", Plat_FloatTime(), out_group, message);
  } else {
    snprintf(out, out_size, "%s", message);
  }

  out[out_size - 1] = '\0';

  return out;
}

DBG_INTERFACE SpewRetval_t DefaultSpewFunc( SpewType_t type, const tchar *pMsg )
{
	// dimhotepus: Guard against nullptr message (UB in printf %s).
	if ( !pMsg )
		pMsg = _T("");

	// dimhotepus: Single call so concurrent spew from several threads does not interleave
	// within a line (stdio locks the stream per call).
	_tprintf( _T("[%.3f] %s"), Plat_FloatTime(), pMsg );

#ifdef _WIN32
	char msg[4096];
	Plat_DebugString( PrefixMessageGroup( msg, "default", pMsg ) );
#endif

	if ( type == SPEW_ASSERT )
	{
#ifndef _WIN32
		// Non-win32
		const bool bRaiseOnAssert = getenv( "RAISE_ON_ASSERT" ) || CommandLine()->HasParm( "-raiseonassert" );
		return bRaiseOnAssert ? SPEW_DEBUGGER : SPEW_CONTINUE;
#elif !defined(NDEBUG)
		// Win32 debug
		return SPEW_DEBUGGER;
#else
		// Win32 release
		const bool bRaiseOnAssert = CommandLine()->HasParm( "-raiseonassert" );
		return bRaiseOnAssert ? SPEW_DEBUGGER : SPEW_CONTINUE;
#endif
	}

	if ( type == SPEW_ERROR )
		return SPEW_ABORT;

	return SPEW_CONTINUE;
}

//-----------------------------------------------------------------------------
DBG_INTERFACE SpewRetval_t DefaultSpewFuncAbortOnAsserts( SpewType_t type, const tchar *pMsg )
{
	SpewRetval_t r = DefaultSpewFunc( type, pMsg );
	if ( type == SPEW_ASSERT )
		r = SPEW_ABORT;
	return r;
}


//-----------------------------------------------------------------------------
// Spew output management.
//-----------------------------------------------------------------------------
void SpewOutputFunc( SpewOutputFunc_t func )
{
	SpewOutputFunc2( func );
}

// dimhotepus: Add ^ which returns old spew.
// NOTE: another thread may still be running the old function after this
// returns, so a spew function must stay valid until the module is unloaded.
SpewOutputFunc_t SpewOutputFunc2( SpewOutputFunc_t func )
{
	return s_SpewOutputFunc.exchange( func ? func : DefaultSpewFunc, std::memory_order_acq_rel );
}

SpewOutputFunc_t GetSpewOutputFunc( void )
{
	// dimhotepus: Should always be set now.
	// dimhotepus: relaxed -> acquire, pairs with the acq_rel exchange in SpewOutputFunc2.
	return s_SpewOutputFunc.load( std::memory_order_acquire );
}

[[noreturn]] void _ExitOnFatalAssert( const tchar* pFile, int line )
{
	// dimhotepus: Only one thread reports and dumps, the others wait for the exit.
	ClaimProcessExitOrPark();

	(void)_SpewMessage( _T("Fatal assert failed: %s, line %d.  Application exiting.\n"), pFile, line );

	// only write out minidumps if we're not in the debugger
	if ( !Plat_IsInDebugSession() )
	{
		char rgchSuffix[512];
		// dimhotepus: _snprintf -> snprintf, standard and always terminates.
		snprintf( rgchSuffix, std::size(rgchSuffix), "fatalassert_%s_%d", SkipToFname( pFile ), line );
		rgchSuffix[ std::size(rgchSuffix) - 1 ] = '\0';
		WriteMiniDump( rgchSuffix );
	}

	DevMsg( 1, _T("_ExitOnFatalAssert\n") );

	// The C99 (unchanged in C11) standard states that assert calls abort and
	// the abort stuff states this about the return code:
	// See https://stackoverflow.com/questions/2862731/when-assert-fails-what-is-the-program-exit-code
	// dimhotepus: Shared exit helper, exit codes unchanged (3 on Windows, 128 + SIGABRT on POSIX).
	ExitProcessAsAbort();
}


//-----------------------------------------------------------------------------
// Templates to assist in validating pointers:
//-----------------------------------------------------------------------------


DBG_INTERFACE void _AssertValidReadPtr( void* ptr, intp count/* = 1*/ ) //-V2009
{
	Assert( !count || ptr );
}

DBG_INTERFACE void _AssertValidWritePtr( void* ptr, intp count/* = 1*/ ) //-V2009
{
	Assert( !count || ptr );
}

DBG_INTERFACE void _AssertValidReadWritePtr( void* ptr, intp count/* = 1*/ ) //-V2009
{
	Assert( !count || ptr );
}

#undef AssertValidStringPtr
DBG_INTERFACE void AssertValidStringPtr( const tchar* ptr, intp /* = 0xFFFFFF */ )
{
	Assert( ptr );
}

//-----------------------------------------------------------------------------
// Should be called only inside a SpewOutputFunc_t, returns groupname, level, color
//-----------------------------------------------------------------------------
const tchar* GetSpewOutputGroup( void )
{
	const SpewInfo_t *pSpewInfo = g_pSpewInfo;
	assert( pSpewInfo );
	if ( pSpewInfo )
		return pSpewInfo->m_pSpewOutputGroup;
	return nullptr;
}

int GetSpewOutputLevel( void )
{
	const SpewInfo_t *pSpewInfo = g_pSpewInfo;
	assert( pSpewInfo );
	if ( pSpewInfo )
		return pSpewInfo->m_nSpewOutputLevel;
	return -1;
}

const Color* GetSpewOutputColor( void )
{
	const SpewInfo_t *pSpewInfo = g_pSpewInfo;
	assert( pSpewInfo );
	if ( pSpewInfo )
		return pSpewInfo->m_pSpewOutputColor;
	return &s_DefaultOutputColor;
}


//-----------------------------------------------------------------------------
// Spew functions
//-----------------------------------------------------------------------------
DBG_INTERFACE void  _SpewInfo( SpewType_t type, const tchar* pFile, int line )
{
	// Only grab the file name. Ignore the path.
	s_pFileName = SkipToFname( pFile );
	s_Line = line;
	s_SpewType = type;
}


// dimhotepus: Renamed internal _SpewMessage overloads to SpewMessageImpl, so
// they are not confused with the exported _SpewMessage and do not use a
// reserved identifier.
static SpewRetval_t SpewMessageImpl( SpewType_t spewType, const tchar *pGroupName, int nLevel, const Color *pColor, const tchar* pMsgFormat, va_list args )
{
	// dimhotepus: Bound recursive spew.
	if ( t_nSpewDepth >= kMaxSpewDepth )
	{
		// Do not route through the spew function again, it is what recursed.
		fputs( "[dbg] Spew recursion limit reached, message dropped.\n", stderr );
		return SPEW_CONTINUE;
	}

	tchar pTempBuffer[5020];
	// dimhotepus: Capacity in characters, the old code mixed sizeof (bytes) and characters.
	// dimhotepus: Removed assert that format length < buffer, truncation is handled.
	constexpr size_t kCapacity = std::size( pTempBuffer );

	/* Printf the file and line for assert only... */
	size_t len = 0;
	if ( spewType == SPEW_ASSERT )
	{
		// dimhotepus: s_pFileName is nullptr when _SpewInfo was not called on this thread.
		len = SafeFormat( pTempBuffer, kCapacity, _T("%s (%d) : "),
			s_pFileName ? s_pFileName : _T("unknown"), s_Line );
	}

	/* Create the message.... */
	// dimhotepus: Offsets and sizes are now clamped, see SafeVFormat.
	// dimhotepus: Formatting errors no longer return SPEW_ABORT without spewing.
	len += SafeVFormat( pTempBuffer + len, kCapacity - len, pMsgFormat, args );
	// dimhotepus: Already guaranteed by SafeVFormat, but make it provable for the
	// compiler (GCC -O2 -Wstringop-overflow false positive on the '\n' append).
	len = std::min( len, kCapacity - 1 );

	// Add \n for assert
	// dimhotepus: Do not add \n twice if Assert already has one.
	if ( spewType == SPEW_ASSERT && len > 0 && pTempBuffer[len - 1] != _T('\n') )
	{
		// dimhotepus: Was "sizeof( pTempBuffer ) - 1" at offset len, overflow.
		// Overwrite the last character when the buffer is full.
		if ( len + 1 >= kCapacity )
			--len;

		pTempBuffer[len++] = _T('\n');
		pTempBuffer[len] = _T('\0');
	}

	assert( len < kCapacity ); /* use normal assert here; to avoid recursion. */
	
	/* direct it to the appropriate target(s) */
	// dimhotepus: nullptr color / group fall back to defaults for spew functions.
	SpewInfo_t spewInfo =
	{
		pColor ? pColor : &s_DefaultOutputColor,
		pGroupName ? pGroupName : _T(""),
		nLevel
	};

	// dimhotepus: Allow recursive spew from spew by restoring old spew info.
	// RAII so state is restored even if a C++ spew function throws.
	struct SpewScope
	{
		explicit SpewScope( SpewInfo_t *info ) : m_pOld( std::exchange( g_pSpewInfo, info ) ) { ++t_nSpewDepth; }
		~SpewScope() { --t_nSpewDepth; g_pSpewInfo = m_pOld; }
		SpewScope( const SpewScope & ) = delete;
		SpewScope &operator=( const SpewScope & ) = delete;
		SpewInfo_t *m_pOld;
	};

	SpewRetval_t ret;
	{
		const SpewScope scope( &spewInfo );
		// dimhotepus: Defensive fallback, should never be nullptr.
		const SpewOutputFunc_t func = GetSpewOutputFunc();
		ret = ( func ? func : DefaultSpewFunc )( spewType, pTempBuffer );
	}

	switch (ret)
	{
	// Asserts put the break into the macro so it occurs in the right place
	case SPEW_DEBUGGER:
		if ( spewType != SPEW_ASSERT )
		{
			DebuggerBreak();
		}
		break;
		
	case SPEW_ABORT:
		// dimhotepus: Shared exit helper, safe when several threads abort at once.
		ExitProcessAsAbort();

	// dimhotepus: Do nothing.
	case SPEW_CONTINUE:
		break;

	default:
		// dimhotepus: Catch missed spew types.
		assert(false);
		// dimhotepus: Unknown values behave as SPEW_CONTINUE.
		ret = SPEW_CONTINUE;
		break;
	}

	return ret;
}

#include "tier0/valve_off.h"

// dimhotepus: static, internal linkage.
static FORCEINLINE SpewRetval_t SpewMessageImpl( SpewType_t spewType, const tchar* pMsgFormat, va_list args )
{
	return SpewMessageImpl( spewType, _T(""), 0, &s_DefaultOutputColor, pMsgFormat, args );
}


//-----------------------------------------------------------------------------
// Spew group lookup. Caller must hold SpewGroupsMutex() (shared or exclusive).
//
// dimhotepus: The old hand-rolled binary search computed "e = m - 1" with
// size_t, which wrapped to SIZE_MAX when the name sorted before the first
// group and then read far out of bounds. std::lower_bound has no such edge.
//-----------------------------------------------------------------------------
// dimhotepus: Returns pointer instead of index via out param.
static SpewGroup_t *LowerBoundSpewGroup( const tchar* pGroupName )
{
	SpewGroup_t *begin = s_pSpewGroups;
	SpewGroup_t *end = s_pSpewGroups + s_GroupCount;

	return std::lower_bound( begin, end, pGroupName,
		[]( const SpewGroup_t &group, const tchar *name )
		{
			return _tcsicmp( group.m_GroupName, name ) < 0;
		} );
}

static const SpewGroup_t *FindSpewGroup( const tchar* pGroupName )
{
	const SpewGroup_t *it = LowerBoundSpewGroup( pGroupName );
	if ( it != s_pSpewGroups + s_GroupCount && !_tcsicmp( it->m_GroupName, pGroupName ) )
		return it;
	return nullptr;
}

// dimhotepus: Group names are stored truncated, so lookups must use the same truncated
// key, otherwise a long name is never found and gets inserted on every call.
static void MakeGroupKey( tchar (&key)[MAX_GROUP_NAME_LENGTH], const tchar *pGroupName )
{
	const size_t length = _tcslen( pGroupName );
	Assert( length < MAX_GROUP_NAME_LENGTH );

	const size_t copy = std::min( length, std::size( key ) - 1 );
	memcpy( key, pGroupName, copy * sizeof( tchar ) );
	key[copy] = _T('\0');
}

//-----------------------------------------------------------------------------
// True if -hushasserts was passed on command line.
//-----------------------------------------------------------------------------
bool HushAsserts()
{
#ifdef DBGFLAG_ASSERT
	// dimhotepus: const. Thread-safe one-time initialization (magic static).
	static const bool s_bHushAsserts = CommandLine()->HasParm( "-hushasserts" );
	return s_bHushAsserts;
#else
	return true;
#endif
}

//-----------------------------------------------------------------------------
// Tests to see if a particular spew is active
//-----------------------------------------------------------------------------
bool IsSpewActive( const tchar* pGroupName, int level )
{
	// dimhotepus: nullptr group uses the default level instead of crashing.
	if ( !pGroupName )
		return s_DefaultLevel.load( std::memory_order_relaxed ) >= level;

	tchar key[MAX_GROUP_NAME_LENGTH];
	MakeGroupKey( key, pGroupName );

	{
		// dimhotepus: Shared lock, readers do not block each other.
		std::shared_lock lock( SpewGroupsMutex() );

		if ( const SpewGroup_t *group = FindSpewGroup( key ) )
			return group->m_Level >= level;
	}

	// If we don't find the spew group, use the default level.
	return s_DefaultLevel.load( std::memory_order_relaxed ) >= level;
}

// dimhotepus: Renamed from IsSpewActive overload (overloaded an extern "C"
// function). Lock-free, reads s_StandardGroupLevels.
static inline bool IsStandardSpewActive( StandardSpewGroup_t group, int level )
{
	if ( static_cast<size_t>(group) >= std::size(s_StandardGroupLevels) )
	{
		// dimhotepus: Condition was inverted, so this never fired.
		AssertMsg( false, "Group index %d is out of range.", static_cast<int>(group) );
		return false;
	}

	const int groupLevel = s_StandardGroupLevels[group].load( std::memory_order_relaxed );

	// If the group was not activated, use the default level.
	return ( groupLevel != kSpewLevelUnset
		? groupLevel
		: s_DefaultLevel.load( std::memory_order_relaxed ) ) >= level;
}

// dimhotepus: FMTFUNCTION removed from definitions, GCC rejects attributes
// on a function definition; the declarations in dbg.h carry them.
SpewRetval_t  _SpewMessage( PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	va_list args;
	va_start( args, pMsgFormat ); //-V2018 //-V2019
	SpewRetval_t ret = SpewMessageImpl( s_SpewType, pMsgFormat, args );
	va_end(args);
	return ret;
}

SpewRetval_t _DSpewMessage( const tchar *pGroupName, int level, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsSpewActive( pGroupName, level ) )
		return SPEW_CONTINUE;

	va_list args;
	va_start( args, pMsgFormat ); //-V2018 //-V2019
	SpewRetval_t ret = SpewMessageImpl( s_SpewType, pGroupName, level, &s_DefaultOutputColor, pMsgFormat, args );
	va_end(args);
	return ret;
}

DBG_INTERFACE SpewRetval_t ColorSpewMessage( SpewType_t type, const Color *pColor, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	va_list args;
	va_start( args, pMsgFormat ); //-V2018 //-V2019
	SpewRetval_t ret = SpewMessageImpl( type, _T(""), 0, pColor, pMsgFormat, args );
	va_end(args);
	return ret;
}

DBG_INTERFACE SpewRetval_t DColorSpewMessage( SpewType_t type, const tchar *pGroupName, const Color *pColor, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	va_list args;
	va_start( args, pMsgFormat ); //-V2018 //-V2019
	SpewRetval_t ret = SpewMessageImpl( type, pGroupName, 0, pColor, pMsgFormat, args );
	va_end(args);
	return ret;
}

// dimhotepus: Shared body for the many "format and spew" entry points below.
#define SE_DBG_SPEW_VARARGS( _type, _group, _level, _color )					\
	va_list args;															\
	va_start( args, pMsgFormat ); /* //-V2018 //-V2019 */					\
	SpewMessageImpl( _type, _group, _level, _color, pMsgFormat, args );	\
	va_end( args )

void Msg( PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	SE_DBG_SPEW_VARARGS( SPEW_MESSAGE, _T(""), 0, &s_DefaultOutputColor );
}

void DMsg( const tchar *pGroupName, int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( pGroupName, level ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_MESSAGE, pGroupName, level, &s_DefaultOutputColor );
}

void MsgV( PRINTF_FORMAT_STRING const tchar *pMsg, va_list arglist )
{
	SpewMessageImpl( SPEW_MESSAGE, pMsg, arglist );
}


void Warning( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	SE_DBG_SPEW_VARARGS( SPEW_WARNING, _T(""), 0, &s_DefaultOutputColor );
}

void DWarning( const tchar *pGroupName, int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( pGroupName, level ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_WARNING, pGroupName, level, &s_DefaultOutputColor );
}

void WarningV( PRINTF_FORMAT_STRING const tchar *pMsg, va_list arglist )
{
	SpewMessageImpl( SPEW_WARNING, pMsg, arglist );
}


void Log( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	SE_DBG_SPEW_VARARGS( SPEW_LOG, _T(""), 0, &s_DefaultOutputColor );
}

void DLog( const tchar *pGroupName, int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsSpewActive( pGroupName, level ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_LOG, pGroupName, level, &s_DefaultOutputColor );
}

void LogV( PRINTF_FORMAT_STRING const tchar *pMsg, va_list arglist )
{
	SpewMessageImpl( SPEW_LOG, pMsg, arglist );
}


void Error( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	SE_DBG_SPEW_VARARGS( SPEW_ERROR, _T(""), 0, &s_DefaultOutputColor );

	// dimhotepus: Fix UB as ERROR must not return.
	abort();
}

void ErrorV( PRINTF_FORMAT_STRING const tchar *pMsg, va_list arglist )
{
	SpewMessageImpl( SPEW_ERROR, pMsg, arglist );

	// dimhotepus: Fix UB as ERROR must not return.
	abort();
}

//-----------------------------------------------------------------------------
// A couple of super-common dynamic spew messages, here for convenience 
// These looked at the "developer" group, print if it's level 1 or higher 
// dimhotepus: Dev* / Con* / Net* use lock-free IsStandardSpewActive.
//-----------------------------------------------------------------------------
void DevMsg( int level, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_DEVELOPER, level ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_MESSAGE, s_pDeveloper, level, &s_DefaultOutputColor );
}

void DevWarning( int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_DEVELOPER, level ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_WARNING, s_pDeveloper, level, &s_DefaultOutputColor );
}

void DevLog( int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_DEVELOPER, level ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_LOG, s_pDeveloper, level, &s_DefaultOutputColor );
}

void DevMsg( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_DEVELOPER, 1 ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_MESSAGE, s_pDeveloper, 1, &s_DefaultOutputColor );
}

void DevWarning( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_DEVELOPER, 1 ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_WARNING, s_pDeveloper, 1, &s_DefaultOutputColor );
}

void DevLog( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_DEVELOPER, 1 ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_LOG, s_pDeveloper, 1, &s_DefaultOutputColor );
}


//-----------------------------------------------------------------------------
// A couple of super-common dynamic spew messages, here for convenience 
// These looked at the "console" group, print if it's level 1 or higher 
//-----------------------------------------------------------------------------
void ConColorMsg( int level, const Color& clr, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_CONSOLE, level ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_MESSAGE, s_pConsole, level, &clr );
}

void ConMsg( int level, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_CONSOLE, level ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_MESSAGE, s_pConsole, level, &s_DefaultOutputColor );
}

void ConWarning( int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_CONSOLE, level ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_WARNING, s_pConsole, level, &s_DefaultOutputColor );
}

void ConLog( int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_CONSOLE, level ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_LOG, s_pConsole, level, &s_DefaultOutputColor );
}

void ConColorMsg( const Color& clr, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_CONSOLE, 1 ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_MESSAGE, s_pConsole, 1, &clr );
}

void ConMsg( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_CONSOLE, 1 ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_MESSAGE, s_pConsole, 1, &s_DefaultOutputColor );
}

void ConWarning( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_CONSOLE, 1 ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_WARNING, s_pConsole, 1, &s_DefaultOutputColor );
}

void ConLog( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_CONSOLE, 1 ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_LOG, s_pConsole, 1, &s_DefaultOutputColor );
}


void ConDColorMsg( const Color& clr, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_CONSOLE, 2 ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_MESSAGE, s_pConsole, 2, &clr );
}

void ConDMsg( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_CONSOLE, 2 ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_MESSAGE, s_pConsole, 2, &s_DefaultOutputColor );
}

void ConDWarning( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_CONSOLE, 2 ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_WARNING, s_pConsole, 2, &s_DefaultOutputColor );
}

void ConDLog( PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_CONSOLE, 2 ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_LOG, s_pConsole, 2, &s_DefaultOutputColor );
}


//-----------------------------------------------------------------------------
// A couple of super-common dynamic spew messages, here for convenience 
// These looked at the "network" group, print if it's level 1 or higher 
//-----------------------------------------------------------------------------
void NetMsg( int level, PRINTF_FORMAT_STRING const tchar* pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_NETWORK, level ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_MESSAGE, s_pNetwork, level, &s_DefaultOutputColor );
}

void NetWarning( int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_NETWORK, level ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_WARNING, s_pNetwork, level, &s_DefaultOutputColor );
}

void NetLog( int level, PRINTF_FORMAT_STRING const tchar *pMsgFormat, ... )
{
	if( !IsStandardSpewActive( GROUP_NETWORK, level ) )
		return;

	SE_DBG_SPEW_VARARGS( SPEW_LOG, s_pNetwork, level, &s_DefaultOutputColor );
}

#undef SE_DBG_SPEW_VARARGS

#include "tier0/valve_on.h"


//-----------------------------------------------------------------------------
// Sets the priority level for a spew group
//-----------------------------------------------------------------------------
void SpewActivate( const tchar* pGroupName, int level )
{
	Assert( pGroupName );
	// dimhotepus: Was dereferenced unconditionally in release.
	if ( !pGroupName )
		return;
	
	// check for the default group first...
	if ((pGroupName[0] == '*') && (pGroupName[1] == '\0'))
	{
		s_DefaultLevel.store(level, std::memory_order_relaxed);
		return;
	}

	// dimhotepus: INT_MIN is the "unset" marker of the standard group cache.
	if ( level == kSpewLevelUnset )
		++level;

	// dimhotepus: Was _tcscpy into a 48-char field guarded only by a debug Assert.
	tchar key[MAX_GROUP_NAME_LENGTH];
	MakeGroupKey( key, pGroupName );

	bool bOutOfMemory = false;
	{
		std::unique_lock lock( SpewGroupsMutex() );

		// Normal case, search in group list using binary search.
		// If not found, grow the list of groups and insert it into the
		// right place to maintain sorted order. Then set the level.
		SpewGroup_t *it = LowerBoundSpewGroup( key );
		const size_t ind = static_cast<size_t>( it - s_pSpewGroups );

		if ( it == s_pSpewGroups + s_GroupCount || _tcsicmp( it->m_GroupName, key ) )
		{
			// not defined yet, insert an entry.
			const size_t newCount = s_GroupCount + 1;
			// dimhotepus: Do not lose the old block (and crash) when realloc fails.
			void *pGrown = s_pSpewGroups
				? PvRealloc( s_pSpewGroups, newCount * sizeof(SpewGroup_t) )
				: PvAlloc( newCount * sizeof(SpewGroup_t) );

			if ( !pGrown )
			{
				bOutOfMemory = true;
			}
			else
			{
				s_pSpewGroups = static_cast<SpewGroup_t*>( pGrown );

				// shift elements down to preserve order
				memmove( &s_pSpewGroups[ind + 1], &s_pSpewGroups[ind],
					( s_GroupCount - ind ) * sizeof(SpewGroup_t) );

				s_GroupCount = newCount;

				memcpy( s_pSpewGroups[ind].m_GroupName, key, sizeof(key) );
				it = &s_pSpewGroups[ind];
			}
		}

		if ( !bOutOfMemory )
		{
			it->m_Level = level;

			// dimhotepus: Update lock-free standard group cache (was index fixups in s_pGroupIndices).
			for ( size_t i = 0; i < std::size(s_pGroupNames); ++i )
			{
				if ( !_tcsicmp( s_pGroupNames[i], key ) )
				{
					s_StandardGroupLevels[i].store( level, std::memory_order_relaxed );
					break;
				}
			}
		}
	}

	// dimhotepus: Never spew under the lock: a spew function may call IsSpewActive (deadlock).
	if ( bOutOfMemory )
	{
		Warning( "SpewActivate: out of memory, spew group '%s' not added.\n", key );
	}
}

void SpewDeactivate()
{
	std::unique_lock lock( SpewGroupsMutex() );

	// dimhotepus: Ensure counter is 0 to be able to reinit.
	s_GroupCount = 0;

	PvFree( s_pSpewGroups );
	s_pSpewGroups = nullptr;

	// dimhotepus: Reset lock-free standard group cache.
	for ( auto &level : s_StandardGroupLevels )
	{
		level.store( kSpewLevelUnset, std::memory_order_relaxed );
	}
}

// If we don't have a function from cmath, then it doesn't link certain floating-point
// functions in and printfs with %f cause runtime errors in the C libraries.
DBG_INTERFACE float CrackSmokingCompiler( float a )
{
	// dimhotepus: static_cast<float>( fabs( a ) ) -> std::fabs( a ), float overload.
	return std::fabs( a );
}

// dimhotepus: Serializes appends to the small helper log files below.
static std::mutex &LogFilesMutex()
{
	static NeverDestroyed<std::mutex> s_mutex;
	return s_mutex.m_value;
}

void* Plat_SimpleLog( const tchar* file, int line )
{
	constexpr char kLogFileName[] = "simple.log";

	// dimhotepus: Serialize appends, warn (once, thread-safe) outside the lock.
	bool bOpenFailed = false, bWriteFailed = false;
	{
		std::lock_guard lock( LogFilesMutex() );

		auto [f, rc] = s1::posix::posix_file_stream_factory::open( kLogFileName, "at+" );
		if ( !rc )
		{
			size_t size;
			std::tie(size, rc) = f.print( "%s:%i\n", file, line );

			// dimhotepus: Was "if (!rc)", i.e. warned on every *successful* write.
			bWriteFailed = static_cast<bool>( rc );
		}
		else
		{
			bOpenFailed = true;
		}
	}

	if ( bWriteFailed )
		DBG_WARNING_ONCE( "Write to '%s' failed.\n", kLogFileName );
	if ( bOpenFailed )
		DBG_WARNING_ONCE( "Unable to open '%s' as 'at+'.\n", kLogFileName );

	return nullptr;
}

#ifdef DBGFLAG_VALIDATE
void ValidateSpew( CValidator &validator )
{
	validator.Push( _T("Spew globals"), NULL, _T("Global") );

	{
		// dimhotepus: AUTO_LOCK -> shared lock, read only.
		std::shared_lock lock( SpewGroupsMutex() );

		validator.ClaimMemory( s_pSpewGroups );
	}

	validator.Pop( );
}
#endif // DBGFLAG_VALIDATE

//-----------------------------------------------------------------------------
// Purpose: For debugging startup times, etc.
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void COM_TimestampedLog( PRINTF_FORMAT_STRING char const *fmt, ... )
{
	// dimhotepus: The function-local statics used to be plain bools / double
	// written from any thread. Settings are now a magic static, mutable state
	// lives under a mutex.
	struct LogSettings
	{
		bool should_log;
		bool should_log_2_etw;
	};
	static const LogSettings settings = []
	{
		const bool etw = CommandLine()->HasParm( "-etwprofile" );
		return LogSettings{ etw || CommandLine()->HasParm( "-profile" ), etw };
	}();

	if ( !settings.should_log )
	{
		return;
	}

	char message[1024];
	va_list argptr;
	va_start( argptr, fmt ); //-V2018 //-V2019
	vsnprintf( message, sizeof( message ), fmt, argptr );
	va_end( argptr );
	// dimhotepus: Explicit termination for pre-C99 CRTs.
	message[ std::size( message ) - 1 ] = '\0';

	constexpr char kLogFileName[] = "timestamped.log";

	// dimhotepus: Save errno at the failure point, report outside the lock.
	int removeErrno = 0;
	bool bOpenFailed = false, bWriteFailed = false;
	{
		std::lock_guard lock( LogFilesMutex() );

		// dimhotepus: Store time as double
		static double last_time_stamp = 0.0;
		// dimhotepus: Renamed from is_first_write, which meant the opposite.
		static bool is_log_file_reset = false;

		const double now_stamp{Plat_FloatTime()};

		// If ETW profiling is enabled then do it only.
		if ( settings.should_log_2_etw )
		{
			ETWMark( message );
		}
		else
		{
			if ( !is_log_file_reset )
			{
				// dimhotepus: std::remove is standard on every platform
				// (unlink is POSIX-only / deprecated on MSVC). A missing file
				// on first run is not an error.
				if ( std::remove( kLogFileName ) != 0 && errno != ENOENT )
				{
					removeErrno = errno;
				}

				is_log_file_reset = true;
			}

			auto [file, rc] = s1::posix::posix_file_stream_factory::open( kLogFileName, "at+" );
			if ( !rc )
			{
				size_t size;
				std::tie(size, rc) =
					file.print( "[%8.4f | %8.4f]:  %s\n", now_stamp, now_stamp - last_time_stamp, message );

				// dimhotepus: Was "if (!rc)", i.e. warned on every *successful* write.
				bWriteFailed = static_cast<bool>( rc );
			}
			else
			{
				bOpenFailed = true;
			}
		}

		last_time_stamp = now_stamp;
	}

	// dimhotepus: Never spew under the lock.
	if ( removeErrno )
	{
		Warning( "Unable to remove timestamp log file '%s': %s.\n",
			kLogFileName,
			std::generic_category().message( removeErrno ).c_str() );
	}
	if ( bWriteFailed )
		DBG_WARNING_ONCE( "Write to '%s' failed.\n", kLogFileName );
	if ( bOpenFailed )
		DBG_WARNING_ONCE( "Unable to open '%s' as 'at+'.\n", kLogFileName );
}

//-----------------------------------------------------------------------------
// Sets an assert failed notify handler
//-----------------------------------------------------------------------------
void SetAssertFailedNotifyFunc( AssertFailedNotifyFunc_t func )
{
	SetAssertFailedNotifyFunc2( func );
}

//-----------------------------------------------------------------------------
// Sets an assert failed notify handler
//-----------------------------------------------------------------------------
AssertFailedNotifyFunc_t SetAssertFailedNotifyFunc2( AssertFailedNotifyFunc_t func )
{
	// dimhotepus: relaxed -> acq_rel, publish the handler to other threads.
	return s_AssertFailedNotifyFunc.exchange( func, std::memory_order_acq_rel );
}


//-----------------------------------------------------------------------------
// Calls the assert failed notify handler if one has been set
//-----------------------------------------------------------------------------
void CallAssertFailedNotifyFunc( const char *pchFile, int nLine, const char *pchMessage )
{
	// dimhotepus: relaxed -> acquire, pairs with SetAssertFailedNotifyFunc2.
	const auto assertFn = s_AssertFailedNotifyFunc.load( std::memory_order_acquire );
	if (assertFn) assertFn( pchFile, nLine, pchMessage );
}