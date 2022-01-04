//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//
//=============================================================================//

#include "winlite.h"

#include <shellapi.h>
#include <memory>
#include <system_error>
#include <variant>

#include "tier0/dbg.h"

// Wide args wrapper.
struct WideArgs
{
	// Construct wide args from command line. |command_line| Command line.
	explicit WideArgs(_In_z_ const wchar_t *command_line) noexcept
			: argv{::CommandLineToArgvW( command_line, &argc )}
	{
	}

	~WideArgs() noexcept
	{
		[[maybe_unused]] const bool is_succeeded{::LocalFree( argv ) == nullptr};
		AssertMsg(is_succeeded, "Failed to LocalFree memory from CommandLineToArgvW.");
	}

	// Argv.
	wchar_t **argv;
	// Argc.
	int argc;
};

/**
 * @brief Converts wchar_t -> UTF-8.
 * @param wide wchar_t source.
 * @param out_size Out UTF-8 size.
 * @return UTF-8 string.
 */
[[nodiscard]] std::variant<std::unique_ptr<char[]>, std::error_code> WideToUtf8(
		_In_z_ const wchar_t *wide, size_t &out_size) noexcept 
{
	out_size = 0;

	// Compute the size of the required buffer.
	const int size{::WideCharToMultiByte( CP_UTF8, 0, wide, -1, nullptr, 0,
																			  nullptr, nullptr )};
	if ( size <= 0 ) [[unlikely]]
	{
		// This should never happen.
		const auto rc = std::error_code{ (int)::GetLastError(), std::system_category() };
		Warning( "Could not find required size for command line arguments as utf8." );
		return rc;
	}

	// Do the actual conversion.
	std::unique_ptr<char[]> argv{std::make_unique<char[]>( static_cast<size_t>(size) )};
	if ( !argv ) [[unlikely]]
	{
		const auto rc = std::error_code{ (int)ERROR_OUTOFMEMORY, std::system_category() };
		Warning( "Could not allocate memory to convert wchar_t {argc, argv} to UTF-8." );
		return rc;
	}

	const int result{::WideCharToMultiByte( CP_UTF8, 0, wide, -1, argv.get(), size,
																				  nullptr, nullptr )};
	if ( result <= 0 ) [[unlikely]] {
		// This should never happen.
		const auto rc = std::error_code{ (int)::GetLastError(), std::system_category() };
		Warning( "Could not convert command line arguments to utf8." );
		return rc;
	}

	out_size = static_cast<size_t>(size);

	return std::move(argv);
}

// Command line arguments.
class Args
{
public:
	// Parse command line and build command line arguments pack. |command_line| Command line.
	[[nodiscard]] static std::variant<Args, std::error_code> FromCommandLine(
			_In_z_ const wchar_t *command_line) noexcept
	{
		// Parse command line to wide {argc, argv}.
		WideArgs wargs{command_line};
		wchar_t **wargv{wargs.argv};
		const int argc{wargs.argc};

		if ( !wargv || argc <= 0 ) [[unlikely]]
		{
			const auto rc = std::error_code{ (int)::GetLastError() , std::system_category() };
			Warning( "Could not parse command line to {argc, argv} tuple." );
			return rc;
		}

		// Convert argv to UTF-8.
		std::unique_ptr<char *[]> argv{std::make_unique<char *[]>( static_cast<size_t>(argc) + 1) };
		if ( !argv ) [[unlikely]]
		{
			const auto rc = std::error_code{ ERROR_OUTOFMEMORY, std::system_category() };
			Warning( "Could not allocate memory to convert wchar_t {argc, argv} to UTF-8." );
			return rc;
		}

		for (size_t i{0}; i < static_cast<size_t>(argc); i++) {
			size_t out_size{0};

			auto utf8_result = WideToUtf8( wargv[i], out_size );
			if ( auto *arg = std::get_if<std::unique_ptr<char[]>>( &utf8_result )) [[likely]]
			{
				argv[i] = arg->release();
			}
			else
			{
				// Can leak argv[0..i-1], but we stop the app anyway so OS reclaims memory.
				return std::get<std::error_code>( utf8_result );
			}
		}

		argv[static_cast<size_t>(argc)] = nullptr;

		return Args{argv.release(), argc};
	}

	Args( Args &&args ) noexcept
			: values_{args.values_}, count_{args.count_} {
		args.values_ = nullptr;
		args.count_ = 0;
	}

	Args &operator=( Args &&args ) noexcept = delete;

	~Args() noexcept {
		for (int i = 0; i < count_; i++) {
			delete[] values_[i];
		}

		delete[] values_;
		count_ = 0;
	}

	// Args count.
	[[nodiscard]] int count() const noexcept { return count_; }

	// Args values.
	[[nodiscard]] char **values() const noexcept { return values_; }

 private:
	// Args values.
	char **values_;
	// Count of args.
	int count_;

	// Construct args.
	Args( _In_ char **values, _In_ int count ) noexcept
			: values_{values}, count_{count}
	{
	}
};
