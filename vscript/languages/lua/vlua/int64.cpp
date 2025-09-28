#include "int64.h"

#include "tier1/strtools.h"

#include <lauxlib.h>

namespace {

constexpr inline char INT64_TYPE[]{"Int64"};
constexpr inline char INT64_NAME[]{"Int64"};


Int64 lua_getint64ByValue( lua_State *pState, int i )
{
	if ( lua_isinteger( pState, i ) )
	{
		const lua_Integer iValue = lua_tointeger( pState, i );

		return Int64{ size_cast<int64>( iValue ) };
	}

	if ( luaL_checkudata( pState, i, INT64_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, INT64_TYPE );
	}

	return *static_cast<Int64 *>( lua_touserdata( pState, i ) );
}


Int64 *lua_allocint64( lua_State *pState )
{
	auto *v = static_cast<Int64 *>( lua_newuserdata( pState, sizeof( Int64 ) ) );
	if ( !v )
	{
		luaL_error( pState, "out of memory when alloc %s", INT64_TYPE );
	}

	luaL_getmetatable( pState, INT64_TYPE );
	lua_setmetatable( pState, -2 );

	return v;
}


int int64_new( lua_State *pState )
{
	lua_settop( pState, 3 );

	Int64 *v = lua_allocint64( pState );
	v->value = luaL_optinteger( pState, 1, 0 );

	return 1;
}


int int64_tostring( lua_State *pState )
{
	char s[ 32 ];
	const Int64 *v = lua_getint64( pState, 1 );

	V_sprintf_safe( s, "%s (%lld)", INT64_TYPE, v->value );

	lua_pushstring( pState, s );

	return 1;
}


int int64_add( lua_State *pState )
{
	const Int64 v1 = lua_getint64ByValue( pState, 1 );
	const Int64 v2 = lua_getint64ByValue( pState, 2 );

	int64 vResult = v1.value + v2.value;

	lua_newint64( pState, vResult );

	return 1;
}


int int64_subtract( lua_State *pState )
{
	const Int64 v1 = lua_getint64ByValue( pState, 1 );
	const Int64 v2 = lua_getint64ByValue( pState, 2 );

	const int64 vResult = v1.value - v2.value;

	lua_newint64( pState, vResult );

	return 1;
}


int int64_multiply( lua_State *pState )
{
	const Int64 v1 = lua_getint64ByValue( pState, 1 );
	const Int64 v2 = lua_getint64ByValue( pState, 2 );

	const int64 vResult = v1.value * v2.value;

	lua_newint64( pState, vResult );

	return 1;
}


int int64_divide( lua_State *pState )
{
	const Int64 v1 = lua_getint64ByValue( pState, 1 );
	const Int64 v2 = lua_getint64ByValue( pState, 2 );

	luaL_argcheck( pState, v2.value == 0, 2, "division by zero" );

	const int64 vResult = v1.value / v2.value;

	lua_newint64( pState, vResult );

	return 1;
}


int int64_equal( lua_State *pState )
{
	const Int64 v1 = lua_getint64ByValue( pState, 1 );
	const Int64 v2 = lua_getint64ByValue( pState, 2 );

	return v1.value == v2.value ? 1 : 0;
}


int int64_bitwiseand( lua_State *pState )
{
	const Int64 v1 = lua_getint64ByValue( pState, 1 );
	const Int64 v2 = lua_getint64ByValue( pState, 2 );

	lua_pushinteger( pState, v1.value & v2.value );

	return 1;
}


int int64_bitwiseor( lua_State *pState )
{
	const Int64 v1 = lua_getint64ByValue( pState, 1 );
	const Int64 v2 = lua_getint64ByValue( pState, 2 );

	lua_pushinteger( pState, v1.value | v2.value );

	return 1;
}


int int64_bitwisexor( lua_State *pState )
{
	const Int64 v1 = lua_getint64ByValue( pState, 1 );
	const Int64 v2 = lua_getint64ByValue( pState, 2 );

	lua_pushinteger( pState, v1.value ^ v2.value );

	return 1;
}


int int64_bitwisenot( lua_State *pState )
{
	const Int64 v1 = lua_getint64ByValue( pState, 1 );

	lua_pushinteger( pState, ~v1.value );

	return 1;
}


int int64_clearbit( lua_State *pState )
{
	Int64 v1 = lua_getint64ByValue( pState, 1 );
	const lua_Integer n = luaL_checkinteger( pState, 2 );
	
	if ( n < 0 || n > 63 )
	{
		char err[64];
		V_sprintf_safe( err, "ClearBit accepts bit value in range [0...63], got %lld",
			static_cast<long long>( n ) );

		return luaL_argerror( pState, 2, err );
	}

	lua_pushinteger( pState, CLEARBITS( v1.value, 1LL << n ) );

	return 1;
}


int int64_isbitset( lua_State *pState )
{
	Int64 v1 = lua_getint64ByValue( pState, 1 );
	const lua_Integer n = luaL_checkinteger( pState, 2 );

	if ( n < 0 || n > 63 )
	{
		char err[64];
		V_sprintf_safe( err, "IsBitSet accepts bit value in range [0...63], got %lld",
			static_cast<long long>( n ) );

		return luaL_argerror( pState, 2, err );
	}

	lua_pushboolean( pState, FBitSet( v1.value, 1LL << n ) > 0 ? 1 : 0 );

	return 1;
}


int int64_setbit( lua_State *pState )
{
	Int64 v1 = lua_getint64ByValue( pState, 1 );
	const lua_Integer n = luaL_checkinteger( pState, 2 );
	
	if ( n < 0 || n > 63 )
	{
		char err[64];
		V_sprintf_safe( err, "SetBit accepts bit value in range [0...63], got %lld",
			static_cast<long long>( n ) );

		return luaL_argerror( pState, 2, err );
	}

	lua_pushinteger( pState, SETBITS( v1.value, 1LL << n ) );

	return 1;
}


int int64_togglebit( lua_State *pState )
{
	Int64 v1 = lua_getint64ByValue( pState, 1 );
	const lua_Integer n = luaL_checkinteger( pState, 2 );
	
	if ( n < 0 || n > 63 )
	{
		char err[64];
		V_sprintf_safe( err, "ToggleBit accepts bit value in range [0...63], got %lld",
			static_cast<long long>( n ) );

		return luaL_argerror( pState, 2, err );
	}

	const long long bitMask = 1LL << n;

	lua_pushinteger( pState,
		FBitSet( v1.value, bitMask ) ? CLEARBITS( v1.value, bitMask ) : SETBITS( v1.value, bitMask ) );

	return 1;
}


int int64_tohexstring( lua_State *pState )
{
	const Int64 v1 = lua_getint64ByValue( pState, 1 );
	
	char hex[64];
	V_sprintf_safe( hex, "0x%llx", v1.value );

	lua_pushstring( pState, hex );

	return 1;
}


constexpr luaL_Reg Int64Functions[] =
{
	{ "__tostring",	int64_tostring	},
	{ "__add",		int64_add		},
	{ "__sub",		int64_subtract	},
	{ "__mul",		int64_multiply	},
	{ "__div",		int64_divide	},
	{ "__eq",		int64_equal	},
	{ "BitwiseAnd",	int64_bitwiseand	},
	{ "BitwiseOr",	int64_bitwiseor	},
	{ "BitwiseXor",	int64_bitwisexor	},
	{ "BitwiseNot",	int64_bitwisenot	},
	{ "ClearBit",	int64_clearbit	},
	{ "IsBitSet",	int64_isbitset	},
	{ "SetBit",		int64_setbit	},
	{ "ToggleBit",	int64_togglebit	},
	{ "ToHexString",int64_tohexstring	},
	{ nullptr,		nullptr			}
};

}  // namespace


int luaopen_int64( lua_State *pState )
{
	luaL_newmetatable( pState, INT64_TYPE );
	luaL_setfuncs( pState, Int64Functions, 0 );
	lua_register( pState, INT64_NAME, int64_new );

	return 1;
}


Int64 *lua_getint64( lua_State *pState, int i )
{
	if ( luaL_checkudata( pState, i, INT64_TYPE ) == nullptr )
	{
		luaL_typeerror( pState, i, INT64_TYPE );
	}

	return static_cast<Int64 *>( lua_touserdata( pState, i ) );
}


Int64 *lua_newint64( lua_State *pState, int64 Value )
{
	Int64 *v = lua_allocint64( pState );

	v->value = Value;

	return v;
}