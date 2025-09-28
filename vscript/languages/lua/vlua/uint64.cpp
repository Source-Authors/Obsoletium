#include "uint64.h"

#include "tier1/strtools.h"

#include <lauxlib.h>

namespace {

constexpr inline char UINT64_TYPE[]{"Uint64"};
constexpr inline char UINT64_NAME[]{"Uint64"};


Uint64 lua_getuint64ByValue( lua_State *pState, int i )
{
	if ( lua_isinteger( pState, i ) )
	{
		const lua_Integer iValue = lua_tointeger( pState, i );

		return Uint64{ size_cast<uint64>( iValue ) };
	}

	if ( luaL_checkudata( pState, i, UINT64_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, UINT64_TYPE );
	}

	return *static_cast<Uint64 *>( lua_touserdata( pState, i ) );
}


Uint64 *lua_allocuint64( lua_State *pState )
{
	auto *v = static_cast<Uint64 *>( lua_newuserdata( pState, sizeof( Uint64 ) ) );
	if ( !v )
	{
		luaL_error( pState, "out of memory when alloc %s", UINT64_TYPE );
	}

	luaL_getmetatable( pState, UINT64_TYPE );
	lua_setmetatable( pState, -2 );

	return v;
}


int uint64_new( lua_State *pState )
{
	lua_settop( pState, 3 );

	Uint64 *v = lua_allocuint64( pState );
	v->value = luaL_optinteger( pState, 1, 0 );

	return 1;
}


int uint64_tostring( lua_State *pState )
{
	char s[ 32 ];
	const Uint64 *v = lua_getuint64( pState, 1 );

	V_sprintf_safe( s, "%s (%llu)", UINT64_TYPE, v->value );

	lua_pushstring( pState, s );

	return 1;
}


int uint64_add( lua_State *pState )
{
	const Uint64 v1 = lua_getuint64ByValue( pState, 1 );
	const Uint64 v2 = lua_getuint64ByValue( pState, 2 );

	uint64 vResult = v1.value + v2.value;

	lua_newuint64( pState, vResult );

	return 1;
}


int uint64_subtract( lua_State *pState )
{
	const Uint64 v1 = lua_getuint64ByValue( pState, 1 );
	const Uint64 v2 = lua_getuint64ByValue( pState, 2 );

	const uint64 vResult = v1.value - v2.value;

	lua_newuint64( pState, vResult );

	return 1;
}


int uint64_multiply( lua_State *pState )
{
	const Uint64 v1 = lua_getuint64ByValue( pState, 1 );
	const Uint64 v2 = lua_getuint64ByValue( pState, 2 );

	const uint64 vResult = v1.value * v2.value;

	lua_newuint64( pState, vResult );

	return 1;
}


int uint64_divide( lua_State *pState )
{
	const Uint64 v1 = lua_getuint64ByValue( pState, 1 );
	const Uint64 v2 = lua_getuint64ByValue( pState, 2 );

	luaL_argcheck( pState, v2.value == 0, 2, "division by zero" );

	const uint64 vResult = v1.value / v2.value;

	lua_newuint64( pState, vResult );

	return 1;
}


int uint64_equal( lua_State *pState )
{
	const Uint64 v1 = lua_getuint64ByValue( pState, 1 );
	const Uint64 v2 = lua_getuint64ByValue( pState, 2 );

	return v1.value == v2.value ? 1 : 0;
}


int uint64_bitwiseand( lua_State *pState )
{
	const Uint64 v1 = lua_getuint64ByValue( pState, 1 );
	const Uint64 v2 = lua_getuint64ByValue( pState, 2 );

	lua_pushinteger( pState, v1.value & v2.value );

	return 1;
}


int uint64_bitwiseor( lua_State *pState )
{
	const Uint64 v1 = lua_getuint64ByValue( pState, 1 );
	const Uint64 v2 = lua_getuint64ByValue( pState, 2 );

	lua_pushinteger( pState, v1.value | v2.value );

	return 1;
}


int uint64_bitwisexor( lua_State *pState )
{
	const Uint64 v1 = lua_getuint64ByValue( pState, 1 );
	const Uint64 v2 = lua_getuint64ByValue( pState, 2 );

	lua_pushinteger( pState, v1.value ^ v2.value );

	return 1;
}


int uint64_bitwisenot( lua_State *pState )
{
	const Uint64 v1 = lua_getuint64ByValue( pState, 1 );

	lua_pushinteger( pState, ~v1.value );

	return 1;
}


int uint64_clearbit( lua_State *pState )
{
	Uint64 v1 = lua_getuint64ByValue( pState, 1 );
	const lua_Integer n = luaL_checkinteger( pState, 2 );
	
	if ( n < 0 || n > 63 )
	{
		char err[64];
		V_sprintf_safe( err, "ClearBit accepts bit value in range [0...63], got %lld",
			static_cast<long long>( n ) );

		return luaL_argerror( pState, 2, err );
	}

	lua_pushinteger( pState, CLEARBITS( v1.value, 1ULL << n ) );

	return 1;
}


int uint64_isbitset( lua_State *pState )
{
	Uint64 v1 = lua_getuint64ByValue( pState, 1 );
	const lua_Integer n = luaL_checkinteger( pState, 2 );

	if ( n < 0 || n > 63 )
	{
		char err[64];
		V_sprintf_safe( err, "IsBitSet accepts bit value in range [0...63], got %lld",
			static_cast<long long>( n ) );

		return luaL_argerror( pState, 2, err );
	}

	lua_pushboolean( pState, FBitSet( v1.value, 1ULL << n ) > 0 ? 1 : 0 );

	return 1;
}


int uint64_setbit( lua_State *pState )
{
	Uint64 v1 = lua_getuint64ByValue( pState, 1 );
	const lua_Integer n = luaL_checkinteger( pState, 2 );
	
	if ( n < 0 || n > 63 )
	{
		char err[64];
		V_sprintf_safe( err, "SetBit accepts bit value in range [0...63], got %lld",
			static_cast<long long>( n ) );

		return luaL_argerror( pState, 2, err );
	}

	lua_pushinteger( pState, SETBITS( v1.value, 1ULL << n ) );

	return 1;
}


int uint64_togglebit( lua_State *pState )
{
	Uint64 v1 = lua_getuint64ByValue( pState, 1 );
	const lua_Integer n = luaL_checkinteger( pState, 2 );
	
	if ( n < 0 || n > 63 )
	{
		char err[64];
		V_sprintf_safe( err, "ToggleBit accepts bit value in range [0...63], got %lld",
			static_cast<long long>( n ) );

		return luaL_argerror( pState, 2, err );
	}

	const unsigned long long bitMask = 1ULL << n;

	lua_pushinteger( pState,
		FBitSet( v1.value, bitMask ) ? CLEARBITS( v1.value, bitMask ) : SETBITS( v1.value, bitMask ) );

	return 1;
}


int uint64_tohexstring( lua_State *pState )
{
	const Uint64 v1 = lua_getuint64ByValue( pState, 1 );
	
	char hex[64];
	V_sprintf_safe( hex, "0x%llx", v1.value );

	lua_pushstring( pState, hex );

	return 1;
}


constexpr luaL_Reg Uint64Functions[] =
{
	{ "__tostring",	uint64_tostring	},
	{ "__add",		uint64_add		},
	{ "__sub",		uint64_subtract	},
	{ "__mul",		uint64_multiply	},
	{ "__div",		uint64_divide	},
	{ "__eq",		uint64_equal	},
	{ "BitwiseAnd",	uint64_bitwiseand	},
	{ "BitwiseOr",	uint64_bitwiseor	},
	{ "BitwiseXor",	uint64_bitwisexor	},
	{ "BitwiseNot",	uint64_bitwisenot	},
	{ "ClearBit",	uint64_clearbit	},
	{ "IsBitSet",	uint64_isbitset	},
	{ "SetBit",		uint64_setbit	},
	{ "ToggleBit",	uint64_togglebit	},
	{ "ToHexString",uint64_tohexstring	},
	{ nullptr,		nullptr			}
};

}  // namespace


int luaopen_uint64( lua_State *pState )
{
	luaL_newmetatable( pState, UINT64_TYPE );
	luaL_setfuncs( pState, Uint64Functions, 0 );
	lua_register( pState, UINT64_NAME, uint64_new );

	return 1;
}


Uint64 *lua_getuint64( lua_State *pState, int i )
{
	if ( luaL_checkudata( pState, i, UINT64_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, UINT64_TYPE );
	}

	return static_cast<Uint64 *>( lua_touserdata( pState, i ) );
}


Uint64 *lua_newuint64( lua_State *pState, uint64 Value )
{
	Uint64 *v = lua_allocuint64( pState );

	v->value = Value;

	return v;
}