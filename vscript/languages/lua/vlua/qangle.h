//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef SE_VSCRIPT_LANGUAGES_LUA_VLUA_QANGLE_H_
#define SE_VSCRIPT_LANGUAGES_LUA_VLUA_QANGLE_H_

#include "mathlib/vector.h"

struct lua_State;

extern int luaopen_qangle( lua_State *L );
extern QAngle *lua_getqangle( lua_State *L, int i );
extern QAngle *lua_newqangle( lua_State *L, const QAngle *Value );

#endif  // !SE_VSCRIPT_LANGUAGES_LUA_VLUA_QANGLE_H_
