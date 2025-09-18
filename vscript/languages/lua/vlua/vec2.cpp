#include "vec2.h"

#include "tier1/strtools.h"
#include "mathlib/mathlib.h"

#include <lua.h>
#include <lauxlib.h>

namespace {

constexpr inline char VECTOR2D_TYPE[]{"Vector2D"};
constexpr inline char VECTOR2D_NAME[]{"Vector2D"};


Vector2D lua_getvec2ByValue( lua_State *pState, int i )
{
	if ( lua_isnumber( pState, i ) )
	{
		const lua_Number flValue = lua_tonumber( pState, i );
		const float flArg = size_cast<float>( flValue );

		return { flArg, flArg };
	}

	if ( luaL_checkudata( pState, i, VECTOR2D_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, VECTOR2D_TYPE );
	}

	return *static_cast<Vector2D *>( lua_touserdata( pState, i ) );
}


Vector2D *lua_allocvec2( lua_State *pState )
{
	auto *v = static_cast<Vector2D *>( lua_newuserdata( pState, sizeof( Vector2D ) ) );
	if ( !v )
	{
		luaL_error( pState, "out of memory when alloc %s", VECTOR2D_TYPE );
	}

	luaL_getmetatable( pState, VECTOR2D_TYPE );
	lua_setmetatable( pState, -2 );

	return v;
}


int vec2_new( lua_State *pState )
{
	lua_settop( pState, 3 );

	Vector2D *v = lua_allocvec2( pState );
	v->x = size_cast<float>( luaL_optnumber( pState, 1, 0 ) );
	v->y = size_cast<float>( luaL_optnumber( pState, 2, 0 ) );

	return 1;
}


int vec2_index( lua_State *pState )
{
	const char *pszKey = luaL_checkstring( pState, 2 );

	if ( pszKey && !Q_isempty( pszKey ) && pszKey[ 1 ] == '\0' )
	{
		const Vector2D *v = lua_getvec2( pState, 1 );
		switch ( pszKey[ 0 ] ) 
		{
			case '1': case 'x': case 'r':
				lua_pushnumber( pState, v->x );
				return 1;

			case '2': case 'y': case 'g':
				lua_pushnumber( pState, v->y );
				return 1;

			default:
			{
				char error[32];
				V_sprintf_safe(error, "unknown %s index %c", VECTOR2D_TYPE, pszKey[0] );
				luaL_argcheck( pState, false, 1, error );
			}
		}
	}

	lua_getfield( pState, LUA_REGISTRYINDEX, VECTOR2D_TYPE );
	lua_pushstring( pState, pszKey );
	lua_rawget( pState, -2 );

	return 1;
}


int vec2_newindex( lua_State *pState ) 
{
	const char *pszKey = luaL_checkstring( pState, 2 );

	if ( pszKey && !Q_isempty( pszKey ) && pszKey[ 1 ] == '\0' )
	{
		Vector2D	*v = lua_getvec2( pState, 1 );
		const lua_Number flValue = luaL_checknumber( pState, 3 );
		switch ( pszKey[ 0 ] ) 
		{
			case '1': case 'x': case 'r':
				v->x = size_cast<float>( flValue ); 
				break;

			case '2': case 'y': case 'g':
				v->y = size_cast<float>( flValue ); 
				break;

			default:
			{
				char error[32];
				V_sprintf_safe(error, "unknown %s index %c", VECTOR2D_TYPE, pszKey[0] );
				luaL_argcheck( pState, false, 1, error );
			}
		}
	}

	return 1;
}


int vec2_tostring( lua_State *pState )
{
	char s[ 64 ];
	const Vector2D *v = lua_getvec2( pState, 1 );

	V_sprintf_safe( s, "%s (%f, %f)", VECTOR2D_TYPE, v->x, v->y );

	lua_pushstring( pState, s );

	return 1;
}


int vec2_add( lua_State *pState )
{
	const Vector2D v1 = lua_getvec2ByValue( pState, 1 );
	const Vector2D v2 = lua_getvec2ByValue( pState, 2 );

	const Vector2D vResult = v1 + v2;

	lua_newvec2( pState, &vResult );

	return 1;
}


int vec2_subtract( lua_State *pState )
{
	const Vector2D v1 = lua_getvec2ByValue( pState, 1 );
	const Vector2D v2 = lua_getvec2ByValue( pState, 2 );

	const Vector2D vResult = v1 - v2;

	lua_newvec2( pState, &vResult );

	return 1;
}


int vec2_multiply( lua_State *pState )
{
	const Vector2D v1 = lua_getvec2ByValue( pState, 1 );
	const Vector2D v2 = lua_getvec2ByValue( pState, 2 );

	const Vector2D vResult = v1 * v2;

	lua_newvec2( pState, &vResult );

	return 1;
}


int vec2_divide( lua_State *pState )
{
	const Vector2D v1 = lua_getvec2ByValue( pState, 1 );
	const Vector2D v2 = lua_getvec2ByValue( pState, 2 );

	const Vector2D vResult = v1 / v2;

	lua_newvec2( pState, &vResult );

	return 1;
}


int vec2_unaryminus( lua_State *pState )
{
	const Vector2D v1 = lua_getvec2ByValue( pState, 1 );

	const Vector2D vResult = -v1;

	lua_newvec2( pState, &vResult );

	return 1;
}


int vec2_length( lua_State *pState )
{
	const Vector2D v1 = lua_getvec2ByValue( pState, 1 );

	const float flResult = v1.Length();

	lua_pushnumber( pState, flResult );

	return 1;
}


int vec2_lerp( lua_State* pState )
{
	const Vector2D v1 = lua_getvec2ByValue( pState, 1 );
	const Vector2D v2 = lua_getvec2ByValue( pState, 2 );
	const lua_Number flValue = luaL_checknumber( pState, 3 );

	const Vector2D vResult = Lerp( size_cast<float>( flValue ), v1, v2 );

	lua_newvec2( pState, &vResult );

	return 1;
}


int vec2_normalized( lua_State *pState )
{
	Vector2D v1 = lua_getvec2ByValue( pState, 1 );

	Vector2DNormalize( v1 );

	lua_newvec2( pState, &v1 );

	return 1;
}


int vec2_equal( lua_State *pState )
{
	const Vector2D v1 = lua_getvec2ByValue( pState, 1 );
	const Vector2D v2 = lua_getvec2ByValue( pState, 2 );

	return v1 == v2 ? 1 : 0;
}


int vec2_dot( lua_State *pState )
{
	const Vector2D v1 = lua_getvec2ByValue( pState, 1 );
	const Vector2D v2 = lua_getvec2ByValue( pState, 2 );

	const float flResult = v1.Dot( v2 );

	lua_pushnumber( pState, flResult );

	return 1;
}


int vec2_cross( lua_State *pState )
{
	const Vector2D v1 = lua_getvec2ByValue( pState, 1 );
	const Vector2D v2 = lua_getvec2ByValue( pState, 2 );

	const float flResult = v1.x * v2.y - v1.y * v2.x;

	lua_pushnumber( pState, flResult );

	return 1;
}


constexpr luaL_Reg Vec2Functions[] =
{
	{ "__index",	vec2_index		},
	{ "__newindex",	vec2_newindex	},
	{ "__tostring",	vec2_tostring	},
	{ "__add",		vec2_add		},
	{ "__sub",		vec2_subtract	},
	{ "__mul",		vec2_multiply	},
	{ "__div",		vec2_divide		},
	{ "__unm",		vec2_unaryminus	},
	{ "__len",		vec2_length		},
	{ "__eq",		vec2_equal		},
	{ "Dot",		vec2_dot		},
	{ "Cross",		vec2_cross		},
	{ "Length",		vec2_length		},
	{ "Lerp",		vec2_lerp		},
	{ "Normalized",	vec2_normalized	},
	{ nullptr,		nullptr			}
};

}  // namespace


int luaopen_vec2( lua_State *pState )
{
	luaL_newmetatable( pState, VECTOR2D_TYPE );
	luaL_setfuncs( pState, Vec2Functions, 0 );
	lua_register( pState, VECTOR2D_NAME, vec2_new );

	return 1;
}


Vector2D *lua_getvec2( lua_State *pState, int i )
{
	if ( luaL_checkudata( pState, i, VECTOR2D_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, VECTOR2D_TYPE );
	}

	return static_cast<Vector2D *>( lua_touserdata( pState, i ) );
}


Vector2D *lua_newvec2( lua_State *pState, const Vector2D *Value )
{
	Vector2D *v = lua_allocvec2( pState );

	v->x = Value->x;
	v->y = Value->y;

	return v;
}