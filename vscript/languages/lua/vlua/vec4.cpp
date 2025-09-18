#include "vec4.h"

#include "tier1/strtools.h"
#include "mathlib/mathlib.h"

#include <lua.h>
#include <lauxlib.h>

namespace {

constexpr inline char VECTOR4D_TYPE[]{"Vector4D"};
constexpr inline char VECTOR4D_NAME[]{"Vector4D"};


Vector4D lua_getvec4ByValue( lua_State *pState, int i )
{
	if ( lua_isnumber( pState, i ) )
	{
		const lua_Number flValue = lua_tonumber( pState, i );
		const float flArg = size_cast<float>( flValue );

		return { flArg, flArg, flArg, flArg };
	}

	if ( luaL_checkudata( pState, i, VECTOR4D_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, VECTOR4D_TYPE );
	}

	return *static_cast<Vector4D *>( lua_touserdata( pState, i ) );
}


Vector4D *lua_allocvec4( lua_State *pState )
{
	auto *v = static_cast<Vector4D *>( lua_newuserdata( pState, sizeof( Vector4D ) ) );
	if ( !v )
	{
		luaL_error( pState, "out of memory when alloc %s", VECTOR4D_TYPE );
	}

	luaL_getmetatable( pState, VECTOR4D_TYPE );
	lua_setmetatable( pState, -2 );

	return v;
}


int vec4_new( lua_State *pState )
{
	lua_settop( pState, 3 );

	Vector4D *v = lua_allocvec4( pState );
	v->x = size_cast<float>( luaL_optnumber( pState, 1, 0 ) );
	v->y = size_cast<float>( luaL_optnumber( pState, 2, 0 ) );
	v->z = size_cast<float>( luaL_optnumber( pState, 3, 0 ) );
	v->w = size_cast<float>( luaL_optnumber( pState, 4, 0 ) );

	return 1;
}


int vec4_index( lua_State *pState )
{
	const char *pszKey = luaL_checkstring( pState, 2 );

	if ( pszKey && !Q_isempty( pszKey ) && pszKey[ 1 ] == '\0' )
	{
		const Vector4D *v = lua_getvec4( pState, 1 );
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
				V_sprintf_safe(error, "unknown %s index %c", VECTOR4D_TYPE, pszKey[0] );
				return luaL_argerror( pState, 2, error );
			}
		}
	}

	lua_getfield( pState, LUA_REGISTRYINDEX, VECTOR4D_TYPE );
	lua_pushstring( pState, pszKey );
	lua_rawget( pState, -2 );

	return 1;
}


int vec4_newindex( lua_State *pState ) 
{
	const char *pszKey = luaL_checkstring( pState, 2 );

	if ( pszKey && !Q_isempty( pszKey ) && pszKey[ 1 ] == '\0' )
	{
		Vector4D	*v = lua_getvec4( pState, 1 );
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
				V_sprintf_safe(error, "unknown %s index %c", VECTOR4D_TYPE, pszKey[0] );
				luaL_argcheck( pState, false, 1, error );
			}
		}
	}

	return 1;
}


int vec4_tostring( lua_State *pState )
{
	char s[ 64 ];
	const Vector4D *v = lua_getvec4( pState, 1 );

	V_sprintf_safe( s, "%s (%f, %f, %f, %f)", VECTOR4D_TYPE, v->x, v->y, v->z, v->w );

	lua_pushstring( pState, s );

	return 1;
}


int vec4_add( lua_State *pState )
{
	const Vector4D v1 = lua_getvec4ByValue( pState, 1 );
	const Vector4D v2 = lua_getvec4ByValue( pState, 2 );

	const Vector4D vResult = v1 + v2;

	lua_newvec4( pState, &vResult );

	return 1;
}


int vec4_subtract( lua_State *pState )
{
	const Vector4D v1 = lua_getvec4ByValue( pState, 1 );
	const Vector4D v2 = lua_getvec4ByValue( pState, 2 );

	const Vector4D vResult = v1 - v2;

	lua_newvec4( pState, &vResult );

	return 1;
}


int vec4_multiply( lua_State *pState )
{
	const Vector4D v1 = lua_getvec4ByValue( pState, 1 );
	const Vector4D v2 = lua_getvec4ByValue( pState, 2 );

	const Vector4D vResult = v1 * v2;

	lua_newvec4( pState, &vResult );

	return 1;
}


int vec4_divide( lua_State *pState )
{
	const Vector4D v1 = lua_getvec4ByValue( pState, 1 );
	const Vector4D v2 = lua_getvec4ByValue( pState, 2 );

	const Vector4D vResult = v1 / v2;

	lua_newvec4( pState, &vResult );

	return 1;
}


int vec4_unaryminus( lua_State *pState )
{
	const Vector4D v1 = lua_getvec4ByValue( pState, 1 );

	const Vector4D vResult = -v1;

	lua_newvec4( pState, &vResult );

	return 1;
}


int vec4_length( lua_State *pState )
{
	const Vector4D v1 = lua_getvec4ByValue( pState, 1 );

	const float flResult = v1.Length();

	lua_pushnumber( pState, flResult );

	return 1;
}


int vec4_length3d( lua_State *pState )
{
	const Vector4D v1 = lua_getvec4ByValue( pState, 1 );
	
	const float flResult = sqrtf( v1.x * v1.x + v1.y * v1.y + v1.z * v1.z );

	lua_pushnumber( pState, flResult );

	return 1;
}


int vec4_length2d( lua_State *pState )
{
	const Vector4D v1 = lua_getvec4ByValue( pState, 1 );
	
	const float flResult = sqrtf( v1.x * v1.x + v1.y * v1.y );

	lua_pushnumber( pState, flResult );

	return 1;
}


int vec4_lerp( lua_State* pState )
{
	const Vector4D v1 = lua_getvec4ByValue( pState, 1 );
	const Vector4D v2 = lua_getvec4ByValue( pState, 2 );
	const lua_Number flValue = luaL_checknumber( pState, 3 );

	const Vector4D vResult = Lerp( size_cast<float>( flValue ), v1, v2 );

	lua_newvec4( pState, &vResult );

	return 1;
}


int vec4_normalized( lua_State *pState )
{
	Vector4D v1 = lua_getvec4ByValue( pState, 1 );

	Vector4DNormalize( v1 );

	lua_newvec4( pState, &v1 );

	return 1;
}


int vec4_equal( lua_State *pState )
{
	const Vector4D v1 = lua_getvec4ByValue( pState, 1 );
	const Vector4D v2 = lua_getvec4ByValue( pState, 2 );

	return v1 == v2 ? 1 : 0;
}


int vec4_dot( lua_State *pState )
{
	const Vector4D v1 = lua_getvec4ByValue( pState, 1 );
	const Vector4D v2 = lua_getvec4ByValue( pState, 2 );

	const float flResult = v1.Dot( v2 );

	lua_pushnumber( pState, flResult );

	return 1;
}


constexpr luaL_Reg Vec4Functions[] =
{
	{ "__index",	vec4_index		},
	{ "__newindex",	vec4_newindex	},
	{ "__tostring",	vec4_tostring	},
	{ "__add",		vec4_add		},
	{ "__sub",		vec4_subtract	},
	{ "__mul",		vec4_multiply	},
	{ "__div",		vec4_divide		},
	{ "__unm",		vec4_unaryminus	},
	{ "__len",		vec4_length		},
	{ "__eq",		vec4_equal		},
	{ "Dot",		vec4_dot		},
	{ "Length",		vec4_length		},
	{ "Length3D",	vec4_length3d	},
	{ "Length2D",	vec4_length2d	},
	{ "Lerp",		vec4_lerp		},
	{ "Normalized",	vec4_normalized	},
	{ nullptr,		nullptr			}
};

}  // namespace


int luaopen_vec4( lua_State *pState )
{
	luaL_newmetatable( pState, VECTOR4D_TYPE );
	luaL_setfuncs( pState, Vec4Functions, 0 );
	lua_register( pState, VECTOR4D_NAME, vec4_new );

	return 1;
}


Vector4D *lua_getvec4( lua_State *pState, int i )
{
	if ( luaL_checkudata( pState, i, VECTOR4D_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, VECTOR4D_TYPE );
	}

	return static_cast<Vector4D *>( lua_touserdata( pState, i ) );
}


Vector4D *lua_newvec4( lua_State *pState, const Vector4D *Value )
{
	Vector4D *v = lua_allocvec4( pState );

	v->x = Value->x;
	v->y = Value->y;
	v->z = Value->z;
	v->w = Value->w;

	return v;
}