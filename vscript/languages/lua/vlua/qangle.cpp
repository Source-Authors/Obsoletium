#include "qangle.h"

#include "mathlib/mathlib.h"

#include "vec3.h"

#include <lua.h>
#include <lauxlib.h>


constexpr char QANGLE_TYPE[]{"QAngle"};
constexpr char QANGLE_NAME[]{"QAngle"};


QAngle *lua_getqangle( lua_State *pState, int i )
{
	if ( luaL_checkudata( pState, i, QANGLE_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, QANGLE_TYPE );
	}

	return static_cast<QAngle *>( lua_touserdata( pState, i ) );
}


QAngle lua_getqangleByValue( lua_State *pState, int i )
{
	if ( lua_isnumber( pState, i ) )
	{
		lua_Number flValue = lua_tonumber( pState, i );

		return QAngle( size_cast<float>( flValue ), size_cast<float>( flValue ), size_cast<float>( flValue ) );
	}
	if ( luaL_checkudata( pState, i, QANGLE_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, QANGLE_TYPE );
	}

	return *( QAngle * )lua_touserdata( pState, i );
}


static QAngle *lua_allocqangle( lua_State *pState )
{
	auto *v = static_cast<QAngle *>( lua_newuserdata( pState, sizeof( QAngle ) ) );
	if ( !v )
	{
		luaL_error( pState, "out of memory when alloc %s", QANGLE_TYPE );
	}

	luaL_getmetatable( pState, QANGLE_TYPE );
	lua_setmetatable( pState, -2 );

	return v;
}


QAngle *lua_newqangle( lua_State *pState, const QAngle *Value )
{
	QAngle *v = lua_allocqangle( pState );

	v->x = Value->x;
	v->y = Value->y;
	v->z = Value->z;

	return v;
}


static int qangle_new( lua_State *pState )
{
	lua_settop( pState, 3 );

	QAngle *v = lua_allocqangle( pState );
	v->x = size_cast<float>( luaL_optnumber( pState, 1, 0 ) );
	v->y = size_cast<float>( luaL_optnumber( pState, 2, 0 ) );
	v->z = size_cast<float>( luaL_optnumber( pState, 3, 0 ) );

	return 1;
}


static int qangle_index( lua_State *pState )
{
	const char	*pszKey = luaL_checkstring( pState, 2 );

	if ( pszKey[ 1 ] == '\0' )
	{
		QAngle	*v = lua_getqangle( pState, 1 );
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
		}
	}

	lua_getfield( pState, LUA_REGISTRYINDEX, QANGLE_TYPE );
	lua_pushstring( pState, pszKey );
	lua_rawget( pState, -2 );

	return 1;
}


static int qangle_newindex( lua_State *pState ) 
{
	const char *pszKey = luaL_checkstring( pState, 2 );

	if ( pszKey[ 1 ] == '\0' )
	{
		QAngle	*v = lua_getqangle( pState, 1 );
		lua_Number	flValue = luaL_checknumber( pState, 3 );
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
				break;
		}
	}

	return 1;
}

static int qangle_tostring( lua_State *pState )
{
	char	s[ 64 ];
	QAngle	*v = lua_getqangle( pState, 1 );

	sprintf( s, "%s (%f, %f, %f)", QANGLE_TYPE, v->x, v->y, v->z );

	lua_pushstring( pState, s );

	return 1;
}


static int qangle_add( lua_State *pState )
{
	QAngle v1 = lua_getqangleByValue( pState, 1 );
	QAngle v2 = lua_getqangleByValue( pState, 2 );

	QAngle vResult = v1 + v2;

	lua_newqangle( pState, &vResult );

	return 1;
}


static int qangle_subtract( lua_State *pState )
{
	QAngle v1 = lua_getqangleByValue( pState, 1 );
	QAngle v2 = lua_getqangleByValue( pState, 2 );

	QAngle vResult = v1 - v2;

	lua_newqangle( pState, &vResult );

	return 1;
}


static int qangle_equal( lua_State *pState )
{
	QAngle v1 = lua_getqangleByValue( pState, 1 );
	QAngle v2 = lua_getqangleByValue( pState, 2 );

	return ( v1 == v2 ? 1 : 0 );
}


static int qangle_forward( lua_State *pState )
{
	QAngle v1 = lua_getqangleByValue( pState, 1 );

	Vector fwd;
	AngleVectors( v1, &fwd, nullptr, nullptr );

	lua_newvec3( pState, &fwd );

	return 1;
}


static int qangle_left( lua_State *pState )
{
	QAngle v1 = lua_getqangleByValue( pState, 1 );

	Vector rh;
	AngleVectors( v1, nullptr, &rh, nullptr );

	lua_newvec3( pState, &rh );

	return 1;
}


static int qangle_up( lua_State *pState )
{
	QAngle v1 = lua_getqangleByValue( pState, 1 );

	Vector up;
	AngleVectors( v1, nullptr, nullptr, &up );

	lua_newvec3( pState, &up );

	return 1;
}


static constexpr luaL_Reg Registrations[] =
{
	{ "__index",	qangle_index	},
	{ "__newindex",	qangle_newindex	},
	{ "__tostring",	qangle_tostring	},
	{ "__add",		qangle_add		},
	{ "__sub",		qangle_subtract	},
	{ "__eq",		qangle_equal	},
	{ "Forward",	qangle_forward	},
	{ "Left",		qangle_left		},
	{ "Up",			qangle_up		},
	{ nullptr,		nullptr			}
};


int luaopen_qangle( lua_State *pState )
{
	luaL_newmetatable( pState, QANGLE_TYPE );
	luaL_setfuncs( pState, Registrations, 0 );
	lua_register( pState, QANGLE_NAME, qangle_new );

	return 1;
}
