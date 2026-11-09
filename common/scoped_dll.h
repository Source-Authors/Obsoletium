// Copyright Valve Corporation, All rights reserved.
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.

#ifndef SE_COMMON_SCOPED_DLL_H_
#define SE_COMMON_SCOPED_DLL_H_

#include <cstdio> // fprintf
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility> // std::swap, std::exchange

#include "tier0/basetypes.h"

#ifdef _WIN32

#include <sal.h>
#include <specstrings.h>

FORWARD_DECLARE_HANDLE( HINSTANCE );
using HMODULE = HINSTANCE;

using FARPROC = intp( __stdcall * )();

extern "C"
{
__declspec( dllimport ) _Ret_maybenull_ HMODULE __stdcall LoadLibraryExA(
	_In_ const char *lpLibFileName, _Reserved_ void *hFile,
	_In_ unsigned long dwFlags );
__declspec( dllimport ) int __stdcall     FreeLibrary( _In_ HMODULE hLibModule );
__declspec( dllimport ) FARPROC __stdcall GetProcAddress(
	_In_ HMODULE hModule, _In_ const char *lpProcName );

__declspec( dllimport )
_Check_return_
_Post_equals_last_error_ unsigned long __stdcall
GetLastError();

} // extern "C"

#else

#include <dlfcn.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstddef>
#include <cstring>

#if defined( __linux__ )
#include <elf.h>
#elif defined( __APPLE__ )
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach/machine.h>
#else
#error "Please define your platform."
#endif

#include "tier0/platform.h" // RunCodeAtScopeExit

#if defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Sorry, Big-endian targets are not supported."
#endif

#if defined( __linux__ )
#if defined( __x86_64__ )
#define SE_EXPECT_ELF_CLASS ELFCLASS64
#define SE_EXPECT_EM        EM_X86_64
#elif defined( __aarch64__ )
#define SE_EXPECT_ELF_CLASS ELFCLASS64
#define SE_EXPECT_EM        EM_AARCH64
#elif defined( __arm__ )
#define SE_EXPECT_ELF_CLASS ELFCLASS32
#define SE_EXPECT_EM        EM_ARM
#else
#error "Sorry, Linux architecture is not supported."
#endif
#else
#if defined( __x86_64__ )
#define SE_EXPECT_APPLE_CPU CPU_TYPE_X86_64
#elif defined( __aarch64__ )
#define SE_EXPECT_APPLE_CPU CPU_TYPE_ARM64
#else
#error "Sorry, macOS architecture is not supported."
#endif
#endif

#endif // !_WIN32

namespace s1
{

// Is function pointer?
template< typename F >
constexpr bool is_function_pointer_v =
std::is_pointer_v< F > && std::is_function_v< std::remove_pointer_t< F > >;

#if defined( __linux__ ) || defined( __APPLE__ )

constexpr inline int kDlOpenFailed{ -1 };
constexpr inline int kDlSymFailed{ -2 };

namespace s1::detail
{

/*
 * dlerror() text is consumed by the first read and overwritten by the next dl*
 * call, so it is captured at the failure site. Kept per thread, like dlerror().
 */
[[nodiscard]] inline std::string &last_dl_error() noexcept
{
	static thread_local std::string message;
	return message;
}

inline void capture_dl_error() noexcept
{
	const char *msg{ ::dlerror() };
	try
	{
		last_dl_error() = msg ? msg : "";
	}
	catch ( ... )
	{
		// Out of memory: keep whatever was there; message() has a fallback.
	}
}

} // namespace s1::detail

/**
 * @brief dl* error category.
 */
class DlErrorCategory : public std::error_category
{
public:
	DlErrorCategory() noexcept = default;

	/**
	 * @brief Error category name.
	 * @return Category name.
	 */
	[[nodiscard]] const char *name() const noexcept override { return "dl"; }

	/**
	 * @brief Gets error value code description.
	 * @param error_value code.
	 * @return Error description (last captured dlerror() text on this thread).
	 */
	[[nodiscard]] std::string message( int error_value ) const override
	{
		if ( error_value == kDlOpenFailed || error_value == kDlSymFailed )
		{
			const std::string &msg{ s1::detail::last_dl_error() };
			if ( !msg.empty() )
				return msg;

			return error_value == kDlOpenFailed ? "dlopen failed" :
												  "dlsym failed";
		}

		return "unknown dl error";
	}
};

/**
 * @brief Gets dl* error category singleton.
 * @return dl* error category singleton.
 */
[[nodiscard]] inline const DlErrorCategory &dl_error_category() noexcept
{
	static const DlErrorCategory dl_error_category;
	return dl_error_category;
}

#if defined( __linux__ )
/* Returns 0 if the file looks like a loadable ELF for this target, else an errno. */
[[nodiscard]] inline int check_format( int fd ) noexcept
{
	unsigned char buf[sizeof( Elf64_Ehdr )];
	ssize_t       n{ pread( fd, buf, sizeof buf, 0 ) };

	if ( n < 0 )
		return errno; /* EIO and similar read errors */
	if ( (size_t)n < EI_NIDENT || memcmp( buf, ELFMAG, SELFMAG ) != 0 )
		return ENOEXEC; /* not ELF, or truncated */
	if ( buf[EI_CLASS] != SE_EXPECT_ELF_CLASS || buf[EI_DATA] != ELFDATA2LSB )
		return ENOEXEC; /* wrong word size or endianness */
	if ( (size_t)n < sizeof( Elf32_Ehdr ) )
		return ENOEXEC; /* truncated header */

	/* e_type and e_machine sit at the same offsets in Elf32_Ehdr and Elf64_Ehdr. */
	Elf32_Ehdr hdr;
	memcpy( &hdr, buf, sizeof hdr );
	if ( hdr.e_type != ET_DYN )
		return ENOEXEC; /* not a shared object */
	if ( hdr.e_machine != SE_EXPECT_EM )
		return ENOEXEC; /* built for another architecture */
	return 0;
}
#else
/* Returns 0 if the file looks like a loadable Mach-O for this target, else an errno. */
[[nodiscard]] inline int check_format( int fd ) noexcept
{
	struct mach_header_64 h;
	ssize_t               n{ pread( fd, &h, sizeof h, 0 ) };

	if ( n < 0 )
		return errno;
	if ( (size_t)n < sizeof h )
		return ENOEXEC;
	if ( h.magic == FAT_MAGIC || h.magic == FAT_CIGAM )
		return 0; /* universal file: dyld picks the slice */
#ifdef FAT_MAGIC_64
	if ( h.magic == FAT_MAGIC_64 || h.magic == FAT_CIGAM_64 )
		return 0; /* 64-bit universal file */
#endif
	if ( h.magic != MH_MAGIC_64 )
		return ENOEXEC; /* not a 64-bit Mach-O */
	if ( h.filetype != MH_DYLIB && h.filetype != MH_BUNDLE )
		return ENOEXEC; /* not a loadable library */
	if ( h.cputype != SE_EXPECT_APPLE_CPU )
		return ENOEXEC; /* built for another architecture */
	return 0;
}
#endif

/*
 * Returns {} on success or an std::error_code value describing the failure.
 * On failure, *handle is nullptr and nothing stays loaded.
 * dl_error_category() errors carry the dlerror() text via message().
 */
[[nodiscard]] inline std::error_code plugin_load( const char *path,
                                                  unsigned    flags,
                                                  void      **handle ) noexcept
{
	*handle = nullptr;
	if ( path == nullptr )
		return std::make_error_code( std::errc::invalid_argument );

	/*
	 * 1. Open and inspect the same file. O_NONBLOCK keeps a FIFO from hanging
	 *    the open; open() itself reports ENOENT/EACCES/ELOOP/etc.
	 */
	const int fd{ open( path, O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOCTTY ) };
	if ( fd < 0 )
	{
		return std::error_code{
			errno, /* ENOENT, ENOTDIR, ENAMETOOLONG, ELOOP, EACCES, EMFILE */
			std::system_category()
		};
	}

	/* 2. Type */
	struct stat st;
	if ( fstat( fd, &st ) != 0 )
	{
		const int e{ errno };
		close( fd );
		return std::error_code{ e, std::system_category() };
	}

	if ( !S_ISREG( st.st_mode ) )
	{
		close( fd );
		return std::error_code{ S_ISDIR( st.st_mode ) ? EISDIR : EINVAL,
			                    std::system_category() };
	}

	/* 3. Format and architecture */
	const int rc{ check_format( fd ) };
	close( fd );
	if ( rc != 0 )
	{
		return std::error_code{ rc, std::system_category() };
	}

	/* 4. Load the library */
	const int old_errno{ errno };
	RunCodeAtScopeExit( errno = old_errno );

	::dlerror(); /* clear any stale error */

	void *h{ ::dlopen( path, static_cast< int >( flags ) ) };
	if ( h == nullptr )
	{
		/*
		 * dlerror() names the actual cause.
		 */
		s1::detail::capture_dl_error();
		return std::error_code{ kDlOpenFailed, dl_error_category() };
	}

	*handle = h;
	return std::error_code{};
}

#endif // __linux__ || __APPLE__

// Scoped DLL.
class ScopedDll
{
public:
	// Create scoped DLL from dll path and load flags.
	// Note: on POSIX, load_flags are dlopen() flags and must include
	// RTLD_LAZY or RTLD_NOW.
	ScopedDll( const char *dll_path, unsigned int load_flags ) noexcept
#ifdef _WIN32
		:
		dll_{ ::LoadLibraryExA( dll_path, nullptr, load_flags ) },
		rc_{ dll_ ? 0 : static_cast< int >( ::GetLastError() ),
		     std::system_category() }
#else
		:
		dll_{ nullptr },
		rc_{ plugin_load( dll_path, load_flags, &dll_ ) },
		dll_path_{ dll_path ? dll_path : "" }
#endif
	{
	}

	ScopedDll( const ScopedDll & ) = delete;
	ScopedDll &operator=( const ScopedDll & ) = delete;

	ScopedDll( ScopedDll &&d ) noexcept :
		dll_{ std::exchange( d.dll_, nullptr ) },
		rc_{ d.rc_ }
#ifndef _WIN32
		,
		dll_path_{ std::move( d.dll_path_ ) }
#endif
	{
	}

	ScopedDll &operator=( ScopedDll &&d ) noexcept
	{
		std::swap( dll_, d.dll_ );
		std::swap( rc_, d.rc_ );
#ifndef _WIN32
		std::swap( dll_path_, d.dll_path_ );
#endif
		return *this;
	}

	~ScopedDll() noexcept
	{
		if ( dll_ )
		{
			// On unload failure, log and leak the handle. Exiting from a
			// destructor (possibly during static teardown) is worse than a
			// library that stays mapped until process exit.
#ifdef _WIN32
			if ( !::FreeLibrary( dll_ ) )
			{
				fprintf( stderr, "Failed to free library (error %lu); leaking handle.\n",
				         ::GetLastError() );
			}
#else
			const int old_errno{ errno };
			RunCodeAtScopeExit( errno = old_errno );

			if ( ::dlclose( dll_ ) != 0 )
			{
				const char *msg{ ::dlerror() };
				fprintf( stderr, "Failed to close the %s: %s; leaking handle.\n",
				         dll_path_.c_str(), msg ? msg : "unknown error" );
			}
#endif
			dll_ = nullptr;
		}
	}

	// Get DLL function by name.
	template< typename F >
	[[nodiscard]] std::tuple< F, std::error_code >
	GetFunction( const char *name ) const noexcept
	{
		static_assert( is_function_pointer_v< F >,
		               "F must be a pointer to function type." );

		// dlsym( nullptr, ... ) means RTLD_DEFAULT on glibc: it would silently
		// search the global scope instead of failing.
		if ( !dll_ || !name )
		{
			return { nullptr,
				     std::make_error_code( std::errc::invalid_argument ) };
		}

#ifdef _WIN32
		F f = reinterpret_cast< F >( ::GetProcAddress( dll_, name ) );
		if ( f == nullptr )
		{
			return { nullptr, std::error_code{ static_cast< int >( ::GetLastError() ), std::system_category() } };
		}
#else
		::dlerror(); /* clear any stale error */

		void *sym{ ::dlsym( dll_, name ) };
		if ( sym == nullptr )
		{
			s1::detail::capture_dl_error();
			return { nullptr, std::error_code{ kDlSymFailed, dl_error_category() } };
		}

		F f = reinterpret_cast< F >( sym );
#endif

		return { f, std::error_code{} };
	}

	// Check DLL loaded.
	[[nodiscard]] bool     operator!() const noexcept { return !dll_; }
	[[nodiscard]] explicit operator bool() const noexcept { return dll_ != nullptr; }

	// Last error code for DLL.
	[[nodiscard]] std::error_code error_code() const noexcept { return rc_; }

private:
#ifdef _WIN32
	HMODULE dll_;
#else
	void *dll_;
#endif

	std::error_code rc_;

#ifndef _WIN32
	std::string dll_path_; // Owned copy: used for diagnostics in ~ScopedDll.
#endif
};

} // namespace s1

#endif // !SE_COMMON_SCOPED_DLL_H_
