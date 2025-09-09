//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef SE_VSCRIPT_LANGUAGES_LUA_VLUA_UINT64_H_
#define SE_VSCRIPT_LANGUAGES_LUA_VLUA_UINT64_H_

#include "tier0/basetypes.h"

struct lua_State;

struct Uint64
{
	uint64 value;

	Uint64() = default;
	explicit Uint64( uint64 v ) : value{v} {}
};

extern int luaopen_uint64( lua_State *L );
extern Uint64 *lua_getuint64( lua_State *L, int i );
extern Uint64 *lua_newuint64( lua_State *L, uint64 Value );

#endif  // !SE_VSCRIPT_LANGUAGES_LUA_VLUA_UINT64_H_
