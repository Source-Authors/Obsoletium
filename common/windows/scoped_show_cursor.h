// Copyright Valve Corporation, All rights reserved.
//
// Scoped show cursor flag.

#ifndef SE_COMMON_WINDOWS_SCOPED_SHOW_CURSOR_H_
#define SE_COMMON_WINDOWS_SCOPED_SHOW_CURSOR_H_

#include "tier0/annotations.h"
#include "tier0/basetypes.h"

FORWARD_DECLARE_HANDLE( HINSTANCE );

extern "C" __declspec( dllimport ) int __stdcall ShowCursor( IN_P BOOL bShow );

namespace s1::win
{

// Toggles cursor on/off in scope.
class ScopedShowCursor
{
public:
	explicit ScopedShowCursor( bool is_shown ) noexcept :
		m_is_shown{ is_shown }
	{
		::ShowCursor( is_shown ? 1 : 0 );
	}
	~ScopedShowCursor() noexcept { ::ShowCursor( m_is_shown ? 0 : 1 ); }

	ScopedShowCursor( ScopedShowCursor & ) = delete;
	ScopedShowCursor &operator=( ScopedShowCursor & ) = delete;
	ScopedShowCursor( ScopedShowCursor && ) = delete;
	ScopedShowCursor &operator=( ScopedShowCursor && ) = delete;

private:
	const bool m_is_shown;
};

} // namespace s1::win

#endif // !SE_COMMON_WINDOWS_SCOPED_SHOW_CURSOR_H_
