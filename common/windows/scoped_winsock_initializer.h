// Copyright Valve Corporation, All rights reserved.

#ifndef SE_COMMON_WINDOWS_SCOPED_WINSOCK_INITIALIZER_H_
#define SE_COMMON_WINDOWS_SCOPED_WINSOCK_INITIALIZER_H_

#include <sal.h>
#include <winsock.h>

#include <system_error>

#include "tier0/dbg.h"

namespace s1::win
{

[[nodiscard]] std::error_code GetSystemError( int errc ) noexcept
{
	return std::error_code{ errc, std::system_category() };
}

class ScopedWinsockInitializer
{
public:
	explicit ScopedWinsockInitializer( unsigned short version ) noexcept
		:
		rc_{ GetSystemError( WSAStartup( version, &wsa_data_ ) ) },
		is_started_{ !rc_.value() }
	{
		if ( !is_started_ )
			return;

		if ( LOBYTE( wsa_data_.wVersion ) != LOBYTE( version ) ||
		     HIBYTE( wsa_data_.wVersion ) != HIBYTE( version ) )
		{
			rc_ = GetSystemError( WSAVERNOTSUPPORTED );
		}
	}
	~ScopedWinsockInitializer() noexcept
	{
		if ( is_started_ && WSACleanup() == SOCKET_ERROR )
		{
			rc_ = GetSystemError( WSAGetLastError() );

			AssertMsg( false, "WSACleanup failed: %s\n", rc_.message().c_str() );
			Warning( "Windows sockets %hu shutdown failure: %s.\n", wsa_data_.wVersion,
			         rc_.message().c_str() );
		}
	}

	ScopedWinsockInitializer( ScopedWinsockInitializer & ) = delete;
	ScopedWinsockInitializer( ScopedWinsockInitializer && ) = delete;
	ScopedWinsockInitializer &operator=( ScopedWinsockInitializer & ) = delete;
	ScopedWinsockInitializer &operator=( ScopedWinsockInitializer && ) = delete;

	[[nodiscard]] std::error_code error_code() const noexcept { return rc_; }

private:
	WSADATA         wsa_data_;
	std::error_code rc_;
	const bool      is_started_;
};

} // namespace s1::win

#endif // !SE_COMMON_WINDOWS_SCOPED_WINSOCK_INITIALIZER_H_
