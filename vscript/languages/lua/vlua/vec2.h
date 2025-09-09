//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef SE_VSCRIPT_LANGUAGES_LUA_VLUA_VEC2_H_
#define SE_VSCRIPT_LANGUAGES_LUA_VLUA_VEC2_H_

#include "mathlib/vector2d.h"

struct lua_State;

extern int luaopen_vec2( lua_State *L );
extern Vector2D *lua_getvec2( lua_State *L, int i );
extern Vector2D *lua_newvec2(lua_State *L, const Vector2D *Value);

#endif  // !SE_VSCRIPT_LANGUAGES_LUA_VLUA_VEC2_H_
