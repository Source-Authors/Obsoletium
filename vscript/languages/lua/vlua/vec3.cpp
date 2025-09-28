#include "vec3.h"

#include "tier1/strtools.h"
#include "mathlib/mathlib.h"

#include <lua.h>
#include <lauxlib.h>

namespace {

constexpr inline char VECTOR3D_TYPE[]{"Vector"};
constexpr inline char VECTOR3D_NAME[]{"Vector"};


Vector lua_getvec3ByValue( lua_State *pState, int i )
{
	if ( lua_isnumber( pState, i ) )
	{
		const lua_Number flValue = lua_tonumber( pState, i );
		const float flArg = size_cast<float>( flValue );

		return { flArg, flArg, flArg };
	}

	if ( luaL_checkudata( pState, i, VECTOR3D_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, VECTOR3D_TYPE );

		return { VEC_T_NAN, VEC_T_NAN, VEC_T_NAN };
	}

	return *static_cast<Vector *>( lua_touserdata( pState, i ) );
}


Vector *lua_allocvec3( lua_State *pState )
{
	auto *v = static_cast<Vector *>( lua_newuserdata( pState, sizeof( Vector ) ) );
	if ( !v )
	{
		luaL_error( pState, "out of memory when alloc %s", VECTOR3D_TYPE );

		return nullptr;
	}

	luaL_getmetatable( pState, VECTOR3D_TYPE );
	lua_setmetatable( pState, -2 );

	return v;
}


int vec3_new( lua_State *pState )
{
	lua_settop( pState, 3 );

	Vector *v = lua_allocvec3( pState );
	if (!v) return 0;

	v->x = size_cast<float>( luaL_optnumber( pState, 1, 0 ) );
	v->y = size_cast<float>( luaL_optnumber( pState, 2, 0 ) );
	v->z = size_cast<float>( luaL_optnumber( pState, 3, 0 ) );

	return 1;
}


int vec3_index( lua_State *pState )
{
	const char *pszKey = luaL_checkstring( pState, 2 );

	if ( pszKey && !Q_isempty( pszKey ) && pszKey[ 1 ] == '\0' )
	{
		const Vector *v = lua_getvec3( pState, 1 );
		if (!v) return 0;

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

			default:
			{
				char error[32];
				V_sprintf_safe(error, "unknown %s index %c", VECTOR3D_TYPE, pszKey[0] );
				luaL_argcheck( pState, false, 1, error );
			}
		}
	}

	lua_getfield( pState, LUA_REGISTRYINDEX, VECTOR3D_TYPE );
	lua_pushstring( pState, pszKey );
	lua_rawget( pState, -2 );

	return 1;
}


int vec3_newindex( lua_State *pState ) 
{
	const char *pszKey = luaL_checkstring( pState, 2 );

	if ( pszKey && !Q_isempty( pszKey ) && pszKey[ 1 ] == '\0' )
	{
		Vector	*v = lua_getvec3( pState, 1 );
		if (!v) return 0;

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

			default:
			{
				char error[32];
				V_sprintf_safe(error, "unknown %s index %c", VECTOR3D_TYPE, pszKey[0] );
				luaL_argcheck( pState, false, 1, error );
			}
		}
	}

	return 1;
}


int vec3_tostring( lua_State *pState )
{
	char s[ 64 ];
	const Vector *v = lua_getvec3( pState, 1 );
	if (!v) return 0;

	V_sprintf_safe( s, "%s (%f, %f, %f)", VECTOR3D_TYPE, v->x, v->y, v->z );

	lua_pushstring( pState, s );

	return 1;
}


int vec3_add( lua_State *pState )
{
	const Vector v1 = lua_getvec3ByValue( pState, 1 );
	const Vector v2 = lua_getvec3ByValue( pState, 2 );

	const Vector vResult = v1 + v2;

	lua_newvec3( pState, &vResult );

	return 1;
}


int vec3_subtract( lua_State *pState )
{
	const Vector v1 = lua_getvec3ByValue( pState, 1 );
	const Vector v2 = lua_getvec3ByValue( pState, 2 );

	const Vector vResult = v1 - v2;

	lua_newvec3( pState, &vResult );

	return 1;
}


int vec3_multiply( lua_State *pState )
{
	const Vector v1 = lua_getvec3ByValue( pState, 1 );
	const Vector v2 = lua_getvec3ByValue( pState, 2 );

	const Vector vResult = v1 * v2;

	lua_newvec3( pState, &vResult );

	return 1;
}


int vec3_divide( lua_State *pState )
{
	const Vector v1 = lua_getvec3ByValue( pState, 1 );
	const Vector v2 = lua_getvec3ByValue( pState, 2 );

	const Vector vResult = v1 / v2;

	lua_newvec3( pState, &vResult );

	return 1;
}


int vec3_unaryminus( lua_State *pState )
{
	const Vector v1 = lua_getvec3ByValue( pState, 1 );

	const Vector vResult = -v1;

	lua_newvec3( pState, &vResult );

	return 1;
}


int vec3_length( lua_State *pState )
{
	const Vector v1 = lua_getvec3ByValue( pState, 1 );

	const float flResult = v1.Length();

	lua_pushnumber( pState, flResult );

	return 1;
}


int vec3_length2d( lua_State *pState )
{
	const Vector v1 = lua_getvec3ByValue( pState, 1 );
	
	const float flResult = sqrtf( v1.x * v1.x + v1.y * v1.y );

	lua_pushnumber( pState, flResult );

	return 1;
}


int vec3_lerp( lua_State* pState )
{
	const Vector v1 = lua_getvec3ByValue( pState, 1 );
	const Vector v2 = lua_getvec3ByValue( pState, 2 );
	const lua_Number flValue = luaL_checknumber( pState, 3 );

	const Vector vResult = Lerp( size_cast<float>( flValue ), v1, v2 );

	lua_newvec3( pState, &vResult );

	return 1;
}


int vec3_normalized( lua_State *pState )
{
	const Vector v1 = lua_getvec3ByValue( pState, 1 );

	const Vector vResult = v1.Normalized();
	
	lua_newvec3( pState, &vResult );

	return 1;
}


int vec3_equal( lua_State *pState )
{
	const Vector v1 = lua_getvec3ByValue( pState, 1 );
	const Vector v2 = lua_getvec3ByValue( pState, 2 );

	return v1 == v2 ? 1 : 0;
}


int vec3_dot( lua_State *pState )
{
	const Vector v1 = lua_getvec3ByValue( pState, 1 );
	const Vector v2 = lua_getvec3ByValue( pState, 2 );

	const float flResult = v1.Dot( v2 );

	lua_pushnumber( pState, flResult );

	return 1;
}


int vec3_cross( lua_State *pState )
{
	const Vector v1 = lua_getvec3ByValue( pState, 1 );
	const Vector v2 = lua_getvec3ByValue( pState, 2 );

	const Vector vResult = v1.Cross( v2 );

	lua_newvec3( pState, &vResult );

	return 1;
}

constexpr luaL_Reg Vec3Functions[] =
{
	{ "__index",	vec3_index		},
	{ "__newindex",	vec3_newindex	},
	{ "__tostring",	vec3_tostring	},
	{ "__add",		vec3_add		},
	{ "__sub",		vec3_subtract	},
	{ "__mul",		vec3_multiply	},
	{ "__div",		vec3_divide		},
	{ "__unm",		vec3_unaryminus	},
	{ "__len",		vec3_length		},
	{ "__eq",		vec3_equal		},
	{ "Dot",		vec3_dot		},
	{ "Cross",		vec3_cross		},
	{ "Length",		vec3_length		},
	{ "Length2D",	vec3_length2d	},
	{ "Lerp",		vec3_lerp		},
	{ "Normalized",	vec3_normalized	},
	{ nullptr,		nullptr			}
};

}  // namespace


int luaopen_vec3( lua_State *pState )
{
	luaL_newmetatable( pState, VECTOR3D_TYPE );
	luaL_setfuncs( pState, Vec3Functions, 0 );
	lua_register( pState, VECTOR3D_NAME, vec3_new );

	return 1;
}


Vector *lua_getvec3( lua_State *pState, int i )
{
	if ( luaL_checkudata( pState, i, VECTOR3D_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, VECTOR3D_TYPE );

		return nullptr;
	}

	return static_cast<Vector *>( lua_touserdata( pState, i ) );
}


Vector *lua_newvec3( lua_State *pState, const Vector *Value )
{
	Vector *v = lua_allocvec3( pState );
	if (!v) return nullptr;

	v->x = Value->x;
	v->y = Value->y;
	v->z = Value->z;

	return v;
}