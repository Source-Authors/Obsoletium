//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef SE_VSCRIPT_LANGUAGES_LUA_VLUA_QUATERNION_H_
#define SE_VSCRIPT_LANGUAGES_LUA_VLUA_QUATERNION_H_

#include "mathlib/vector.h"

struct lua_State;

extern int luaopen_quat( lua_State *L );
extern Quaternion *lua_getquat( lua_State *L, int i );
extern Quaternion *lua_newquat( lua_State *L, const Quaternion *Value );

#endif  // !SE_VSCRIPT_LANGUAGES_LUA_VLUA_QUATERNION_H_
