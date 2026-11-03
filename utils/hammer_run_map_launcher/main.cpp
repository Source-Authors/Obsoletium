// Copyright Valve Corporation, All rights reserved.
//
// Purpose: A very simple wrapper to run Hammer map compile commands.

#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cerrno>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#include <direct.h>
#include <process.h>
#else
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "tier0/platform.h"

namespace
{

// Remove surrounding quotes from string view
[[nodiscard]] std::string_view RemoveQuotes( std::string_view str )
{
	if ( str.length() >= 2 && str.front() == '"' && str.back() == '"' )
	{
		return str.substr( 1, str.length() - 2 );
	}
	return str;
}

// Handler function type for local commands
using CommandHandler = std::function< intptr_t( std::string_view ) >;

// Local command registry
class LocalCommandRegistry
{
private:
	std::unordered_map< std::string, CommandHandler > handlers{};

	// Helper: convert command name to lowercase
	[[nodiscard]] static std::string NormalizeCommandName( std::string_view name )
	{
		std::string result{ name };
		std::transform( result.begin(), result.end(), result.begin(),
		                []( unsigned char c )
		                { return static_cast< unsigned char >( std::tolower( c ) ); } );
		return result;
	}

public:
	// Register a local command handler
	void Register( std::string_view name, CommandHandler handler )
	{
		handlers[NormalizeCommandName( name )] = handler;
	}

	// Check if command is registered
	[[nodiscard]] bool IsRegistered( std::string_view command ) const
	{
		// Extract first token (command name)
		size_t end{ command.find_first_of( " \t" ) };
		if ( end == std::string_view::npos )
			end = command.length();

		std::string cmd_name{ NormalizeCommandName( command.substr( 0, end ) ) };
		return handlers.count( cmd_name ) > 0;
	}

	// Execute a registered local command
	[[nodiscard]] intptr_t Execute( std::string_view command ) const
	{
		// Extract first token
		size_t end{ command.find_first_of( " \t" ) };
		if ( end == std::string_view::npos )
			end = command.length();

		std::string cmd_name{ NormalizeCommandName( command.substr( 0, end ) ) };

		auto it{ handlers.find( cmd_name ) };
		if ( it != handlers.end() )
		{
			return it->second( command );
		}

		return EINVAL;
	}
};

// Handler implementations
[[nodiscard]] intptr_t HandleCdCommand( std::string_view command )
{
	// Extract everything after "cd" or "chdir"
	const size_t cmd_end{ command.find_first_of( " \t" ) };
	if ( cmd_end == std::string_view::npos )
	{
		std::cerr << "error: cd requires a path argument.\n";
		return EINVAL;
	}

	std::string_view rest{ command.substr( cmd_end ) };

	// Skip leading whitespace
	const size_t start{ rest.find_first_not_of( " \t" ) };
	if ( start == std::string_view::npos )
	{
		std::cerr << "error: cd requires a path argument.\n";
		return EINVAL;
	}

	rest = rest.substr( start );
	rest = RemoveQuotes( rest );

	// Remove trailing whitespace
	const size_t end{ rest.find_last_not_of( " \t" ) };
	if ( end != std::string_view::npos )
		rest = rest.substr( 0, end + 1 );

	std::string path_str{ rest };

#ifdef _WIN32
	if ( _chdir( path_str.c_str() ) != 0 )
	{
		const int err{ errno };
		std::cerr << "error: chdir \"" << path_str << "\" failed: "
				  << std::strerror( err ) << ".\n";
		return err;
	}
#else
	if ( chdir( path_str.c_str() ) != 0 )
	{
		const int err{ errno };
		std::cerr << "error: chdir \"" << path_str << "\" failed: "
				  << std::strerror( err ) << ".\n";
		return err;
	}
#endif

	return 0;
}

[[nodiscard]] intptr_t HandleSetCommand( std::string_view command )
{
	// Example: set VAR=value
	const size_t cmd_end{ command.find_first_of( " \t" ) };
	if ( cmd_end == std::string_view::npos )
	{
		std::cerr << "error: set requires VAR=value\n";
		return EINVAL;
	}

	std::string_view rest{ command.substr( cmd_end ) };
	const size_t     start{ rest.find_first_not_of( " \t" ) };
	if ( start == std::string_view::npos )
	{
		std::cerr << "error: set requires VAR=value\n";
		return EINVAL;
	}

	std::string env_str{ rest.substr( start ) };

#ifdef _WIN32
	if ( _putenv( env_str.c_str() ) != 0 )
	{
		const int err{ errno };
		std::cerr << "error: _putenv failed: " << std::strerror( err ) << ".\n";
		return err;
	}
#else
	// On Unix, parse and use setenv.
	const size_t eq{ env_str.find( '=' ) };
	if ( eq == std::string::npos )
	{
		std::cerr << "error: set requires VAR=value format\n";
		return EINVAL;
	}

	std::string var{ env_str.substr( 0, eq ) };
	std::string val{ env_str.substr( eq + 1 ) };

	if ( setenv( var.c_str(), val.c_str(), 1 ) != 0 )
	{
		const int err{ errno };
		std::cerr << "error: setenv failed: " << std::strerror( err ) << ".\n";
		return err;
	}
#endif

	return 0;
}

[[nodiscard]] intptr_t HandleEchoCommand( std::string_view command )
{
	// Example: echo message
	const size_t cmd_end{ command.find_first_of( " \t" ) };
	if ( cmd_end == std::string_view::npos )
	{
		std::cout << '\n';
		return 0;
	}

	std::string_view message{ command.substr( cmd_end ) };
	const size_t     start{ message.find_first_not_of( " \t" ) };
	if ( start != std::string_view::npos )
	{
		message = message.substr( start );
		std::cout << message << '\n';
	}

	return 0;
}

[[nodiscard]] intptr_t HandleCopyCommand( std::string_view command )
{
	// Example: copy source destination
	const size_t cmd_end{ command.find_first_of( " \t" ) };
	if ( cmd_end == std::string_view::npos )
	{
		std::cerr << "error: copy requires source and destination arguments.\n";
		return EINVAL;
	}

	std::string_view rest{ command.substr( cmd_end ) };
	const size_t     start{ rest.find_first_not_of( " \t" ) };
	if ( start == std::string_view::npos )
	{
		std::cerr << "error: copy requires source and destination arguments.\n";
		return EINVAL;
	}

	rest = rest.substr( start );

	// Parse source path
	std::string_view source{};
	if ( rest.front() == '"' )
	{
		const size_t end_quote{ rest.find( '"', 1 ) };
		if ( end_quote == std::string_view::npos )
		{
			std::cerr << "error: copy: unterminated quoted source path.\n";
			return EINVAL;
		}
		source = rest.substr( 1, end_quote - 1 );
		rest = rest.substr( end_quote + 1 );
	}
	else
	{
		const size_t end_source{ rest.find_first_of( " \t" ) };
		source = rest.substr( 0, end_source );
		rest = ( end_source != std::string_view::npos ) ? rest.substr( end_source ) : std::string_view{};
	}

	// Skip whitespace
	const size_t dest_start{ rest.find_first_not_of( " \t" ) };
	if ( dest_start == std::string_view::npos )
	{
		std::cerr << "error: copy requires destination argument.\n";
		return EINVAL;
	}

	rest = rest.substr( dest_start );

	// Parse destination path
	std::string_view destination{};
	if ( rest.front() == '"' )
	{
		const size_t end_quote{ rest.find( '"', 1 ) };
		if ( end_quote == std::string_view::npos )
		{
			std::cerr << "error: copy: unterminated quoted destination path.\n";
			return EINVAL;
		}
		destination = rest.substr( 1, end_quote - 1 );
	}
	else
	{
		const size_t end_dest{ rest.find_first_of( " \t" ) };
		destination = rest.substr( 0, end_dest );
	}

	std::error_code ec{};
	std::filesystem::copy_file( source, destination,
	                            std::filesystem::copy_options::overwrite_existing, ec );

	if ( ec )
	{
		std::cerr << "error: copy \"" << source << "\" to \"" << destination
				  << "\" failed: " << ec.message() << ".\n";
		return ec.value();
	}

	return 0;
}

[[nodiscard]] intptr_t HandleDelCommand( std::string_view command )
{
	// Example: del file
	const size_t cmd_end{ command.find_first_of( " \t" ) };
	if ( cmd_end == std::string_view::npos )
	{
		std::cerr << "error: del requires a file argument.\n";
		return EINVAL;
	}

	std::string_view rest{ command.substr( cmd_end ) };
	const size_t     start{ rest.find_first_not_of( " \t" ) };
	if ( start == std::string_view::npos )
	{
		std::cerr << "error: del requires a file argument.\n";
		return EINVAL;
	}

	rest = rest.substr( start );

	// Parse file path
	std::string_view file{};
	if ( rest.front() == '"' )
	{
		const size_t end_quote{ rest.find( '"', 1 ) };
		if ( end_quote == std::string_view::npos )
		{
			std::cerr << "error: del: unterminated quoted file path.\n";
			return EINVAL;
		}
		file = rest.substr( 1, end_quote - 1 );
	}
	else
	{
		const size_t end_file{ rest.find_first_of( " \t" ) };
		file = rest.substr( 0, end_file );
	}

	std::error_code ec{};
	const bool      exists{ std::filesystem::exists( file, ec ) };

	if ( ec )
	{
		std::cerr << "error: del \"" << file << "\" failed: " << ec.message() << ".\n";
		return ec.value();
	}

	if ( !exists )
	{
		std::cerr << "error: del \"" << file << "\": file not found.\n";
		return ENOENT;
	}

	std::filesystem::remove( file, ec );

	if ( ec )
	{
		std::cerr << "error: del \"" << file << "\" failed: " << ec.message() << ".\n";
		return ec.value();
	}

	return 0;
}

[[nodiscard]] intptr_t HandleRenCommand( std::string_view command )
{
	// Example: ren oldname newname
	const size_t cmd_end{ command.find_first_of( " \t" ) };
	if ( cmd_end == std::string_view::npos )
	{
		std::cerr << "error: ren requires old name and new name arguments.\n";
		return EINVAL;
	}

	std::string_view rest{ command.substr( cmd_end ) };
	const size_t     start{ rest.find_first_not_of( " \t" ) };
	if ( start == std::string_view::npos )
	{
		std::cerr << "error: ren requires old name and new name arguments.\n";
		return EINVAL;
	}

	rest = rest.substr( start );

	// Parse old name
	std::string_view old_name{};
	if ( rest.front() == '"' )
	{
		const size_t end_quote{ rest.find( '"', 1 ) };
		if ( end_quote == std::string_view::npos )
		{
			std::cerr << "error: ren: unterminated quoted old name.\n";
			return EINVAL;
		}
		old_name = rest.substr( 1, end_quote - 1 );
		rest = rest.substr( end_quote + 1 );
	}
	else
	{
		const size_t end_old{ rest.find_first_of( " \t" ) };
		old_name = rest.substr( 0, end_old );
		rest = ( end_old != std::string_view::npos ) ? rest.substr( end_old ) : std::string_view{};
	}

	// Skip whitespace
	const size_t new_start{ rest.find_first_not_of( " \t" ) };
	if ( new_start == std::string_view::npos )
	{
		std::cerr << "error: ren requires new name argument.\n";
		return EINVAL;
	}

	rest = rest.substr( new_start );

	// Parse new name
	std::string_view new_name{};
	if ( rest.front() == '"' )
	{
		const size_t end_quote{ rest.find( '"', 1 ) };
		if ( end_quote == std::string_view::npos )
		{
			std::cerr << "error: ren: unterminated quoted new name.\n";
			return EINVAL;
		}
		new_name = rest.substr( 1, end_quote - 1 );
	}
	else
	{
		const size_t end_new{ rest.find_first_of( " \t" ) };
		new_name = rest.substr( 0, end_new );
	}

	std::error_code ec{};
	const bool      exists{ std::filesystem::exists( old_name, ec ) };

	if ( ec )
	{
		std::cerr << "error: ren \"" << old_name << "\" failed: " << ec.message() << ".\n";
		return ec.value();
	}

	if ( !exists )
	{
		std::cerr << "error: ren \"" << old_name << "\": file not found.\n";
		return ENOENT;
	}

	std::filesystem::rename( old_name, new_name, ec );

	if ( ec )
	{
		std::cerr << "error: ren \"" << old_name << "\" to \"" << new_name
				  << "\" failed: " << ec.message() << ".\n";
		return ec.value();
	}

	return 0;
}

// Portable case-insensitive string comparison.
[[nodiscard]] int StrcaseCmp( const char *s1, const char *s2 )
{
	while ( *s1 && *s2 )
	{
		const int c1{ std::tolower( static_cast< unsigned char >( *s1++ ) ) };
		const int c2{ std::tolower( static_cast< unsigned char >( *s2++ ) ) };
		if ( c1 != c2 )
			return c1 - c2;
	}
	return static_cast< unsigned char >( *s1 ) - static_cast< unsigned char >( *s2 );
}

// Decode hex string (each byte as 2 nibbles: 'a'-'p')
[[nodiscard]] std::string DecodeCommand( const std::string &encoded )
{
	if ( encoded.empty() || encoded.size() % 2 != 0 )
		return {};

	std::string result{};
	result.reserve( encoded.size() / 2 );

	for ( size_t i{ 0 }; i < encoded.size(); i += 2 )
	{
		const unsigned char lower{ static_cast< unsigned char >( encoded[i] ) };
		const unsigned char upper{ static_cast< unsigned char >( encoded[i + 1] ) };

		// Validate: must be 'a' to 'p'
		if ( lower < 'a' || lower > 'p' || upper < 'a' || upper > 'p' )
			return {};

		const unsigned char byte{ static_cast< unsigned char >( ( ( upper - 'a' ) << 4 ) | ( lower - 'a' ) ) };
		result += static_cast< char >( byte );
	}

	return result;
}

// Helper: properly escape and quote commands for the shell.
[[nodiscard]] std::string ShellQuoteCommand( std::string_view cmd )
{
#ifdef _WIN32
	// Windows cmd.exe: wrap in double quotes, escape internal quotes.
	std::string result{ "\"" };
	for ( char c : cmd )
	{
		if ( c == '"' )
			result.append( "\"\"" ); // Escape quote by doubling
		else
			result += c;
	}
	result += "\"";
	return result;
#else
	// Unix sh: wrap in single quotes, escape internal single quotes.
	std::string result{ "'" };
	for ( char c : cmd )
	{
		if ( c == '\'' )
			result.append( "'\\''" ); // End quote, escaped quote, start quote
		else
			result += c;
	}
	result += '\'';
	return result;
#endif
}

// Cross-platform process spawning.
[[nodiscard]] intptr_t Spawn( std::string_view full_command )
{
	if ( full_command.empty() )
	{
		errno = EINVAL;
		return -1;
	}

#ifdef _WIN32
	const std::string cmd_quoted{ "\"" + std::string( full_command ) + "\"" };
	// Windows: cmd.exe /S /C command (need quoting for shell interpretation)
	// /S always removes the first and last " and never removes any other ", nor interprets escapes.
	const intptr_t rc{ _spawnlp( _P_WAIT, "cmd.exe", "cmd.exe", "/S", "/C", cmd_quoted.data(), nullptr ) };
	if ( rc == -1 )
	{
		const int err{ errno };
		std::cerr << "error: _spawnlp failed: " << std::strerror( err ) << ".\n";
		return err;
	}
	return rc;
#else
	const std::string cmd_quoted{ ShellQuoteCommand( full_command ) };
	// Unix: /bin/sh -c 'command' (need quoting for shell interpretation)
	const pid_t pid{ fork() };
	if ( pid == -1 )
	{
		const int err{ errno };
		std::cerr << "error: fork failed: " << std::strerror( err ) << ".\n";
		return err;
	}

	if ( pid == 0 )
	{
		execl( "/bin/sh", "sh", "-c", cmd_quoted.c_str(), nullptr );

		const int err{ errno };
		std::cerr << "error: execl failed: " << std::strerror( err ) << ".\n";
		_exit( err > 127 ? 127 : err );
	}

	int status{ 0 };
	if ( waitpid( pid, &status, 0 ) == -1 )
	{
		const int err{ errno };
		std::cerr << "error: waitpid failed: " << std::strerror( err ) << ".\n";
		return err;
	}

	if ( WIFEXITED( status ) )
		return WEXITSTATUS( status );
	else if ( WIFSIGNALED( status ) )
		return WTERMSIG( status ) + 128;

	return -1;
#endif
}

// Wait for user keypress (cross-platform).
void WaitForKeypress()
{
#ifdef _WIN32
	std::cout << "Press any key to continue...\n";
	const int ch{ _getch() };
	if ( ch == -1 )
	{
		std::cerr << "error: _getch failed\n";
	}
#else
	struct termios old_settings{};
	struct termios new_settings{};

	if ( tcgetattr( STDIN_FILENO, &old_settings ) != 0 )
	{
		const int err{ errno };
		std::cerr << "error: tcgetattr failed: " << std::strerror( err ) << ".\n";
		return;
	}

	new_settings = old_settings;
	new_settings.c_lflag &= ~ICANON; // Disable line buffering

	if ( tcsetattr( STDIN_FILENO, TCSANOW, &new_settings ) != 0 )
	{
		const int err{ errno };
		std::cerr << "error: tcsetattr(TCSANOW) failed: " << std::strerror( err ) << ".\n";
		return;
	}

	RunCodeAtScopeExit(
	{
		if ( tcsetattr( STDIN_FILENO, TCSANOW, &old_settings ) != 0 )
		{
			const int err{ errno };
			std::cerr << "error: tcsetattr(restore) failed: " << std::strerror( err ) << ".\n";
		}
	} );

	std::cout << "Press any key to continue...\n";
	std::cin.get();
#endif
}

// Get error message (cross-platform).
[[nodiscard]] std::string GetErrorMessage( int error_code )
{
#ifdef _WIN32
	return std::generic_category().message( error_code );
#else
	return std::strerror( error_code );
#endif
}

} // namespace

int main( int argc, char **argv )
{
	if ( argc < 2 )
	{
		std::cerr << "usage: " << argv[0] << " [encoded_command...] [-WaitForKeypress]\n";
		return EINVAL;
	}

	// Initialize local command registry
	LocalCommandRegistry local_commands{};

	// Register built-in local commands
	local_commands.Register( "cd", HandleCdCommand );
	local_commands.Register( "copy", HandleCopyCommand );
	local_commands.Register( "chdir", HandleCdCommand );
	local_commands.Register( "del", HandleDelCommand );
	local_commands.Register( "echo", HandleEchoCommand );
	local_commands.Register( "ren", HandleRenCommand );
	local_commands.Register( "set", HandleSetCommand );

	bool should_wait_for_keypress{ false };
	RunCodeAtScopeExit(
	{
		if ( should_wait_for_keypress )
		{
			WaitForKeypress();
		}
	} );

	for ( int i{ 1 }; i < argc; ++i )
	{
		const char *arg{ argv[i] };

		// Check for wait flag
		if ( StrcaseCmp( arg, "-WaitForKeypress" ) == 0 )
		{
			should_wait_for_keypress = true;
			continue;
		}

		// Decode command.
		const std::string command{ DecodeCommand( arg ) };
		if ( command.empty() )
		{
			std::cerr << "error: invalid or empty command: "
					  << arg << "\n";
			return EINVAL;
		}

		std::cerr << "exec: " << command << '\n';

		intptr_t rc{};

		// Check if it's a registered local command.
		if ( local_commands.IsRegistered( command ) )
		{
			rc = local_commands.Execute( command );
		}
		else
		{
			// Spawn subprocess for external commands.
			rc = Spawn( command );
		}

		if ( rc == -1 )
		{
			const int err{ errno };
			std::cerr << "error: failed to execute \"" << command
					  << "\": " << GetErrorMessage( err ) << ".\n";
			return err;
		}

		if ( rc != 0 )
		{
			std::cerr << "error: command failed with exit code: " << rc << ".\n";
			return static_cast< int >( rc );
		}
	}

	return EXIT_SUCCESS;
}
