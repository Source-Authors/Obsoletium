//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef SE_VSCRIPT_LANGUAGES_LUA_VLUA_INT64_H_
#define SE_VSCRIPT_LANGUAGES_LUA_VLUA_INT64_H_

#include "tier0/basetypes.h"

struct lua_State;

struct Int64
{
	int64 value;

	Int64() = default;
	explicit Int64( int64 v ) : value{v} {}
};

extern int luaopen_int64( lua_State *L );
extern Int64 *lua_getint64( lua_State *L, int i );
extern Int64 *lua_newint64( lua_State *L, int64 Value );

#endif  // !SE_VSCRIPT_LANGUAGES_LUA_VLUA_INT64_H_
