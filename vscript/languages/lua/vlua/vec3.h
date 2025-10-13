//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef SE_VSCRIPT_LANGUAGES_LUA_VLUA_VEC3_H_
#define SE_VSCRIPT_LANGUAGES_LUA_VLUA_VEC3_H_

#include "mathlib/vector.h"

struct lua_State;

extern int luaopen_vec3( lua_State *L );
extern Vector *lua_getvec3( lua_State *L, int i );
extern Vector *lua_newvec3( lua_State *L, const Vector *Value );

#endif  // !SE_VSCRIPT_LANGUAGES_LUA_VLUA_VEC3_H_
