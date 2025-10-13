//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef SE_VSCRIPT_LANGUAGES_LUA_VLUA_VEC4_H_
#define SE_VSCRIPT_LANGUAGES_LUA_VLUA_VEC4_H_

#include "mathlib/vector4d.h"

struct lua_State;

extern int luaopen_vec4( lua_State *L );
extern Vector4D *lua_getvec4( lua_State *L, int i );
extern Vector4D *lua_newvec4( lua_State *L, const Vector4D *Value );

#endif  // !SE_VSCRIPT_LANGUAGES_LUA_VLUA_VEC4_H_
