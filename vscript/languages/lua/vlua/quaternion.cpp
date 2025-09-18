#include "quaternion.h"

#include "tier1/strtools.h"
#include "mathlib/mathlib.h"

#include <lua.h>
#include <lauxlib.h>

namespace {

constexpr inline char QUATERNION_TYPE[]{"Quaternion"};
constexpr inline char QUATERNION_NAME[]{"Quaternion"};


Quaternion lua_getquaternionByValue( lua_State *pState, int i )
{
	if ( lua_isnumber( pState, i ) )
	{
		const lua_Number flValue = lua_tonumber( pState, i );
		const float flArg = size_cast<float>( flValue );

		return { flArg, flArg, flArg, flArg };
	}

	if ( luaL_checkudata( pState, i, QUATERNION_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, QUATERNION_TYPE );
	}

	return *static_cast<Quaternion *>( lua_touserdata( pState, i ) );
}


Quaternion *lua_allocquaternion( lua_State *pState )
{
	auto *v = static_cast<Quaternion *>( lua_newuserdata( pState, sizeof( Quaternion ) ) );
	if ( !v )
	{
		luaL_error( pState, "out of memory when alloc %s", QUATERNION_TYPE );
	}

	luaL_getmetatable( pState, QUATERNION_TYPE );
	lua_setmetatable( pState, -2 );

	return v;
}


int quaternion_new( lua_State *pState )
{
	lua_settop( pState, 3 );

	Quaternion *v = lua_allocquaternion( pState );
	v->x = size_cast<float>( luaL_optnumber( pState, 1, 0 ) );
	v->y = size_cast<float>( luaL_optnumber( pState, 2, 0 ) );
	v->z = size_cast<float>( luaL_optnumber( pState, 3, 0 ) );
	v->w = size_cast<float>( luaL_optnumber( pState, 4, 0 ) );

	return 1;
}


int quaternion_index( lua_State *pState )
{
	const char *pszKey = luaL_checkstring( pState, 2 );

	if ( pszKey && !Q_isempty( pszKey ) && pszKey[ 1 ] == '\0' )
	{
		const Quaternion *v = lua_getquat( pState, 1 );
		switch ( pszKey[ 0 ] ) 
		{
			case '1': case 'x': case 'r':
				lua_pushnumber( pState, v->x );
				return 1;

			case '2': case 'y': case 'g':
				lua_pushnumber( pState, v->y );
				return 1;

			case '3': case 'z': case 'b':
				lua_pushnumber( pState, v->z );
				return 1;

			case '4': case 'w': case 'a':
				lua_pushnumber( pState, v->w );
				return 1;

			default:
			{
				char error[32];
				V_sprintf_safe(error, "unknown %s index %c", QUATERNION_TYPE, pszKey[0] );
				luaL_argcheck( pState, false, 1, error );
			}
		}
	}

	lua_getfield( pState, LUA_REGISTRYINDEX, QUATERNION_TYPE );
	lua_pushstring( pState, pszKey );
	lua_rawget( pState, -2 );

	return 1;
}


int quaternion_newindex( lua_State *pState ) 
{
	const char *pszKey = luaL_checkstring( pState, 2 );

	if ( pszKey && !Q_isempty( pszKey ) && pszKey[ 1 ] == '\0' )
	{
		Quaternion	*v = lua_getquat( pState, 1 );
		const lua_Number flValue = luaL_checknumber( pState, 3 );
		switch ( pszKey[ 0 ] ) 
		{
			case '1': case 'x': case 'r':
				v->x = size_cast<float>( flValue ); 
				break;

			case '2': case 'y': case 'g':
				v->y = size_cast<float>( flValue ); 
				break;

			case '3': case 'z': case 'b':
				v->z = size_cast<float>( flValue ); 
				break;
				
			case '4': case 'w': case 'a':
				v->w = size_cast<float>( flValue ); 
				break;

			default:
			{
				char error[32];
				V_sprintf_safe(error, "unknown %s index %c", QUATERNION_TYPE, pszKey[0] );
				luaL_argcheck( pState, false, 1, error );
			}
		}
	}

	return 1;
}


int quaternion_tostring( lua_State *pState )
{
	char s[ 64 ];
	const Quaternion *v = lua_getquat( pState, 1 );

	V_sprintf_safe( s, "%s (%f %f %f %f)", QUATERNION_TYPE, v->x, v->y, v->z, v->w );

	lua_pushstring( pState, s );

	return 1;
}


int quaternion_slerp( lua_State* pState )
{
	const Quaternion p = lua_getquaternionByValue( pState, 1 );
	const Quaternion q = lua_getquaternionByValue( pState, 2 );
	const lua_Number flValue = luaL_checknumber( pState, 3 );

	Quaternion qt;
	QuaternionSlerp( p, q, size_cast<float>( flValue ), qt );

	lua_newquat( pState, &qt );

	return 1;
}


int quaternion_blend( lua_State* pState )
{
	const Quaternion p = lua_getquaternionByValue( pState, 1 );
	const Quaternion q = lua_getquaternionByValue( pState, 2 );
	const lua_Number flValue = luaL_checknumber( pState, 3 );

	Quaternion qt;
	QuaternionBlend( p, q, size_cast<float>( flValue ), qt );

	lua_newquat( pState, &qt );

	return 1;
}


int quaternion_scale( lua_State* pState )
{
	const Quaternion p = lua_getquaternionByValue( pState, 1 );
	const lua_Number flValue = luaL_checknumber( pState, 2 );

	Quaternion qt;
	QuaternionScale( p, size_cast<float>( flValue ), qt );

	lua_newquat( pState, &qt );

	return 1;
}


int quaternion_align( lua_State* pState )
{
	const Quaternion p = lua_getquaternionByValue( pState, 1 );
	const Quaternion q = lua_getquaternionByValue( pState, 2 );

	Quaternion qt;
	QuaternionAlign( p, q, qt );

	lua_newquat( pState, &qt );

	return 1;
}


int quaternion_dot( lua_State* pState )
{
	const Quaternion p = lua_getquaternionByValue( pState, 1 );
	const Quaternion q = lua_getquaternionByValue( pState, 2 );

	const float flValue = QuaternionDotProduct( p, q );

	lua_pushnumber( pState, flValue );

	return 1;
}


int quaternion_conjugate( lua_State* pState )
{
	const Quaternion p = lua_getquaternionByValue( pState, 1 );

	Quaternion qt;
	QuaternionConjugate( p, qt );

	lua_newquat( pState, &qt );

	return 1;
}


int quaternion_invert( lua_State* pState )
{
	const Quaternion p = lua_getquaternionByValue( pState, 1 );

	Quaternion qt;
	QuaternionInvert( p, qt );

	lua_newquat( pState, &qt );

	return 1;
}


int quaternion_normalized( lua_State* pState )
{
	Quaternion p = lua_getquaternionByValue( pState, 1 );

	const float flValue = QuaternionNormalize( p );

	lua_pushnumber( pState, flValue );

	return 1;
}


int quaternion_equal( lua_State *pState )
{
	const Quaternion v1 = lua_getquaternionByValue( pState, 1 );
	const Quaternion v2 = lua_getquaternionByValue( pState, 2 );

	return v1 == v2 ? 1 : 0;
}


int quaternion_add( lua_State* pState )
{
	const Quaternion p = lua_getquaternionByValue( pState, 1 );
	const Quaternion q = lua_getquaternionByValue( pState, 2 );

	Quaternion qt;
	QuaternionAdd( p, q, qt );

	lua_newquat( pState, &qt );

	return 1;
}


int quaternion_multiply( lua_State* pState )
{
	const Quaternion p = lua_getquaternionByValue( pState, 1 );
	const Quaternion q = lua_getquaternionByValue( pState, 2 );

	Quaternion qt;
	QuaternionMult( p, q, qt );

	lua_newquat( pState, &qt );

	return 1;
}


constexpr luaL_Reg quaternionFunctions[] =
{
	{ "__index",	quaternion_index	},
	{ "__newindex",	quaternion_newindex	},
	{ "__tostring",	quaternion_tostring	},
	{ "__eq",		quaternion_equal	},
	{ "__add",		quaternion_add		},
	{ "__mul",		quaternion_multiply	},
	{ "SLerp",		quaternion_slerp	},
	{ "Blend",		quaternion_blend	},
	{ "Scale",		quaternion_scale	},
	{ "Align",		quaternion_align	},
	{ "Dot",		quaternion_dot		},
	{ "Conjugate",	quaternion_conjugate	},
	{ "Invert",		quaternion_invert	},
	{ "Normalized",	quaternion_normalized	},
	{ nullptr,		nullptr				}
};

}  // namespace


int luaopen_quat( lua_State *pState )
{
	luaL_newmetatable( pState, QUATERNION_TYPE );
	luaL_setfuncs( pState, quaternionFunctions, 0 );
	lua_register( pState, QUATERNION_NAME, quaternion_new );

	return 1;
}


Quaternion *lua_getquat( lua_State *pState, int i )
{
	if ( luaL_checkudata( pState, i, QUATERNION_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, QUATERNION_TYPE );
	}

	return static_cast<Quaternion *>( lua_touserdata( pState, i ) );
}


Quaternion *lua_newquat( lua_State *pState, const Quaternion *Value )
{
	Quaternion *v = lua_allocquaternion( pState );

	v->x = Value->x;
	v->y = Value->y;
	v->z = Value->z;
	v->w = Value->w;

	return v;
}