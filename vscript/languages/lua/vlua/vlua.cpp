//========== Copyright � 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#if defined(POSIX)
#include <curses.h>
#include <unistd.h>
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <lobject.h>
#include <lstate.h>
#include <ldo.h>

#include "vec3.h"

#include "datamap.h"
#include "tier0/platform.h"
#include "tier1/functors.h"
#include "tier1/utlvector.h"
#include "tier1/utlhash.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlmap.h"
#include "tier1/fmtstr.h"
#include "tier1/convar.h"
#include "mathlib/vector.h"
#include "vstdlib/random.h"

#include "vscript/ivscript.h"

//#include "init_nut.h"

#include "memdbgon.h"

#ifdef VLUA_DEBUG_SERIALIZATION
static int lastType;
#endif


//-------------------------------------------------------------------------
// Helpers
//-------------------------------------------------------------------------
#if 0
static const char *FieldTypeToString( int type )
{
	switch( type )
	{
		case FIELD_VOID:		return "void";
		case FIELD_FLOAT:		return "float";
		case FIELD_CSTRING:		return "string";
		case FIELD_VECTOR:		return "Vector";
		case FIELD_INTEGER:		return "int";
		case FIELD_BOOLEAN:		return "bool";
		case FIELD_CHARACTER:	return "char";
		case FIELD_HSCRIPT:		return "handle";
		default:				return "<unknown>";
	}
}


static const char *LUATypeToString( int nLUAType )
{
	switch( nLUAType )
	{
		case LUA_TNUMBER:			return "FLOAT";
		case LUA_TBOOLEAN:			return "BOOL";
		case LUA_TSTRING:			return "STRING";
		case LUA_TNIL:				return "NULL";
		case LUA_TTABLE:			return "TABLE";
		case LUA_TUSERDATA:			return "USERDATA";
		case LUA_TTHREAD:			return "THREAD";
	}
	return "<unknown>";
}
#endif


class CLuaVM final : public IScriptVM
{
	lua_State			*m_LuaState;
	ScriptOutputFunc_t	m_OutputFunc;
	ScriptErrorFunc_t	m_ErrorFunc;
	lua_CFunction		m_OldPanicFunc;
	int64				m_iUniqueIdSerialNumber;

	struct InstanceContext_t
	{
		void				*pInstance;
		ScriptClassDesc_t	*pClassDesc;
		char				szName[ 64 ];
	};

public:
	explicit CLuaVM( lua_State *pState = nullptr )
	{
		m_LuaState = pState;

		m_OutputFunc = nullptr;
		m_ErrorFunc = nullptr;
		m_OldPanicFunc = nullptr;

		m_iUniqueIdSerialNumber = 0;
	}

	static int PrintFunc( lua_State *pState ) 
	{
		ScriptOutputFunc_t	m_OutputFunc = *( ( ScriptOutputFunc_t * )lua_touserdata( pState, lua_upvalueindex( 1 ) ) );
		CUtlString			Output;

		int n = lua_gettop( pState );  /* number of arguments */
		lua_getglobal( pState, "towstring" );
		lua_getglobal( pState, "tostring" );
		for ( int i = 1; i <= n; i++ ) 
		{
			lua_pushvalue( pState, -1 );  /* function to be called */
			lua_pushvalue( pState, i );   /* value to print */
			lua_call( pState, 1, 1 );
			const char *s = lua_tostring( pState, -1 );  /* get result */
			if ( s == nullptr )
			{
				return luaL_error( pState, "'tostring' must return a string to 'print'" );
			}

			if ( i > 1 ) 
			{
				Output += "\t";
			}

			if ( s )
			{
				Output += s;
			}

			lua_pop( pState, 1 );  /* pop result */
		}

		if ( m_OutputFunc )
		{
			m_OutputFunc( Output );
		}
		else
		{
			Msg( "%s\n", Output.Get() );
		}

		return 0;
	}

	void HandleError( const char *pszErrorText ) const
	{
		if ( m_ErrorFunc )
		{
			m_ErrorFunc( SCRIPT_LEVEL_WARNING, pszErrorText );
		}
		else
		{
			Warning( pszErrorText );
		}
	}

	static int FatalErrorHandler( lua_State *pState )
	{
		const char *err = lua_tostring( pState, 1 );

		Warning( "Fatal lua error:\n%s\n" );

		throw err;
	}

	void FatalError( const char *pszError ) const
	{
		if ( m_ErrorFunc )
		{
			m_ErrorFunc( SCRIPT_LEVEL_ERROR, pszError );
		}
		else
		{
			Warning( pszError );
		}
	}

	intp GetStackSize() const
	{
		return ( ( char * )m_LuaState->stack_last.p - ( char * )m_LuaState->top.p ) / sizeof( TValue );
	}

	static void stackDump (lua_State *L) 
	{
		int top = lua_gettop(L);
		for (int i = 1; i <= top; i++) {  /* repeat for each level */
			int t = lua_type(L, i);

			switch (t) {
			  case LUA_TSTRING:  /* strings */
				  printf("`%s'", lua_tostring(L, i));
				  break;

			  case LUA_TBOOLEAN:  /* booleans */
				  printf(lua_toboolean(L, i) ? "true" : "false");
				  break;

			  case LUA_TNUMBER:  /* numbers */
				  printf("%g", lua_tonumber(L, i));
				  break;

			  default:  /* other values */
				  printf("%s", lua_typename(L, t));
				  break;
			}

			printf("  ");  /* put a separator */
		}
		printf("\n");  /* end the listing */
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	static void PushVariant( lua_State *pState, const ScriptVariant_t &value )
	{
		switch ( value.m_type )
		{
			case FIELD_VOID:
				lua_pushnil( pState );
				break;

			case FIELD_FLOAT:
				lua_pushnumber( pState, value.m_float );
				break;

			case FIELD_CSTRING:
				lua_pushstring( pState, value );
				break;

			case FIELD_VECTOR:
				lua_newvec3( pState, value.m_pVector );
				break;

			case FIELD_INTEGER:
				lua_pushinteger( pState, value.m_int ); 
				break;

			case FIELD_BOOLEAN:
				lua_pushboolean( pState, value.m_bool ); 
				break;

			case FIELD_CHARACTER:
				{
					char sz[2]{value.m_char, '\0'};
					lua_pushlstring( pState, sz, 1 );
					break; 
				}

			case FIELD_HSCRIPT:
				if ( !value.m_hScript )
				{
					lua_pushnil( pState );
				}
				else
				{
					lua_rawgeti( pState, LUA_REGISTRYINDEX, ( intp )value.m_hScript );
					Assert( lua_isnil( pState, -1 ) == false );
				}
				break;
		}
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	static bool ConvertToVariant( int nStackIndex, lua_State *pState, ScriptVariant_t *pReturn )
	{
		switch ( lua_type( pState, nStackIndex ) )
		{
			case LUA_TNIL:
				pReturn->m_type = FIELD_VOID;
				break;
			case LUA_TNUMBER:
				*pReturn = size_cast<float>( lua_tonumber( pState, nStackIndex ) );
				break;
			case LUA_TBOOLEAN:
				*pReturn = lua_toboolean( pState, nStackIndex ) != 0;
				break;
			case LUA_TSTRING:
				{
					const char *string = lua_tostring( pState, nStackIndex );
					const intp size = V_strlen( string ) + 1;
					char *buffer = new char[ size ];
					pReturn->m_pszString = buffer;
					pReturn->m_type = FIELD_CSTRING;
					pReturn->m_flags |= SV_FREE;
					memcpy( buffer, string, size );
				}
				break;
			default:
				{
					Assert( nStackIndex == -1 );
					pReturn->m_type = FIELD_HSCRIPT;
					pReturn->m_hScript = ( HSCRIPT )( intp )luaL_ref( pState, LUA_REGISTRYINDEX );
				}
		}
		return true;
	}

	static void ReleaseVariant( lua_State *pState, ScriptVariant_t &value )
	{
		if ( value.m_type == FIELD_HSCRIPT )
		{
			luaL_unref( pState, LUA_REGISTRYINDEX, size_cast<int>( ( intp )value.m_hScript ) );
		}
		else
		{
			value.Free();
		}
		value.m_type = FIELD_VOID;
	}

	bool Init() override
	{
		m_LuaState = luaL_newstate();
		luaL_openlibs( m_LuaState );
		luaopen_vec3( m_LuaState );

		m_OldPanicFunc = lua_atpanic( m_LuaState, FatalErrorHandler );

		SetOutputCallback( nullptr );

		return true;
	}

	void Shutdown() override
	{
		lua_atpanic( m_LuaState, m_OldPanicFunc );

		if ( m_LuaState )
		{
			lua_close( m_LuaState );
			m_LuaState = nullptr;
		}
	}

	bool ConnectDebugger() override
	{
		Assert( 0 );
		return false;
	}

	void DisconnectDebugger() override
	{
		Assert( 0 );
	}

	ScriptLanguage_t GetLanguage() override
	{
		return SL_LUA;
	}

	const char *GetLanguageName() override
	{
		return "Lua";
	}

	void AddSearchPath( const char *pszSearchPath ) override
	{
		lua_getglobal( m_LuaState, "package" );
		if ( !lua_istable( m_LuaState, -1 ) )
		{
			Assert( 0 );
			lua_pop( m_LuaState, 1 );
			return;
		}

		lua_getfield( m_LuaState, -1, "path" );
		if ( !lua_isstring( m_LuaState, -1 ) )
		{
			Assert( 0 );
			lua_pop( m_LuaState, 1 );
			return;
		}

		CUtlString szNewPath;
		szNewPath = lua_tostring( m_LuaState, -1 ); 
		szNewPath += ";";
		szNewPath += pszSearchPath;
		szNewPath += "\\?.lua";

		lua_pushstring( m_LuaState, szNewPath );
		lua_setfield( m_LuaState, -3, "path" );

		lua_pop( m_LuaState, 2 );
	}

	bool Frame( float simTime ) override
	{
		if ( m_LuaState )
		{
			Msg( "Garbage Collecting...\n" );
			lua_gc( m_LuaState, LUA_GCCOLLECT, 0 );
		}

#if 0
		if ( m_hDbg )
		{
			sq_rdbg_update( m_hDbg );
			if ( !m_hDbg->IsConnected() )
				DisconnectDebugger();
		}
#endif

		return true;
	}

	ScriptStatus_t Run( const char *pszScript, bool bWait = true ) override
	{
		Assert( 0 );
		return SCRIPT_ERROR;
	}

	HSCRIPT CompileScript( const char *pszScript, const char *pszId = nullptr ) override
	{
		int nResult = luaL_loadbuffer( m_LuaState, pszScript, strlen( pszScript ), pszId );

		if ( nResult == 0 )
		{
			int func_ref = luaL_ref( m_LuaState, LUA_REGISTRYINDEX );

//			lua_rawgeti( m_LuaState, LUA_REGISTRYINDEX, func_ref );
//			lua_call( m_LuaState, 0, 0 );

			return ( HSCRIPT )( intp )func_ref;
		}
		else
		{
			const char *pszErrorText = lua_tostring( m_LuaState, -1 );
			HandleError( pszErrorText );
		}

		return INVALID_HSCRIPT;
	}

	void ReleaseScript( HSCRIPT hScript ) override
	{
		luaL_unref( m_LuaState, LUA_REGISTRYINDEX, size_cast<int>( ( intp )hScript ) );
	}

	ScriptStatus_t Run( HSCRIPT hScript, HSCRIPT hScope = NULL, bool bWait = true ) override
	{
		lua_rawgeti( m_LuaState, LUA_REGISTRYINDEX, ( intp )hScript );
		int nResult = lua_pcall( m_LuaState, 0, LUA_MULTRET, 0 );

		switch( nResult )
		{
			case LUA_ERRRUN:
				{
					const char *pszErrorText = lua_tostring( m_LuaState, -1 );
					HandleError( pszErrorText );
					return SCRIPT_ERROR;
				}

			case LUA_ERRMEM:
				{
					const char *pszErrorText = lua_tostring( m_LuaState, -1 );
					HandleError( pszErrorText );
					return SCRIPT_ERROR;
				}

			case LUA_ERRERR:
				{
					const char *pszErrorText = lua_tostring( m_LuaState, -1 );
					HandleError( pszErrorText );
					return SCRIPT_ERROR;
				}
		}

		return SCRIPT_DONE;
	}

	ScriptStatus_t Run( HSCRIPT hScript, bool bWait ) override
	{
		Assert( 0 );
		return SCRIPT_ERROR;
	}

	// stack good
	HSCRIPT CreateScope( const char *pszScope, HSCRIPT hParent = nullptr ) override
	{
		return nullptr;
	}

	void ReleaseScope( HSCRIPT hScript ) override
	{
	}

	// stack good
	HSCRIPT LookupFunction( const char *pszFunction, HSCRIPT hScope = nullptr ) override
	{
		if ( hScope )
		{
			lua_rawgeti( m_LuaState, LUA_REGISTRYINDEX, ( intp )hScope );
			if ( lua_isnil( m_LuaState, -1 ) )
			{
				Assert( 0 );
				lua_pop( m_LuaState, 1 );
				return nullptr;
			}

			lua_getfield( m_LuaState, -1, pszFunction );
			if ( lua_isnil( m_LuaState, -1 ) || !lua_isfunction( m_LuaState, -1 ) )
			{
				lua_pop( m_LuaState, 2 );
				return nullptr;
			}

			int func_ref = luaL_ref( m_LuaState, LUA_REGISTRYINDEX );
			lua_pop( m_LuaState, 1 );

			return ( HSCRIPT )( intp )func_ref;
		}

		lua_getglobal(m_LuaState, pszFunction);
		if ( lua_isnil( m_LuaState, -1 ) || !lua_isfunction( m_LuaState, -1 ) )
		{
			lua_pop( m_LuaState, 1 );
			return nullptr;
		}

		int func_ref = luaL_ref( m_LuaState, LUA_REGISTRYINDEX );

		return ( HSCRIPT )( intp )func_ref;
	}

	// stack good
	void ReleaseFunction( HSCRIPT hScript ) override
	{
		luaL_unref( m_LuaState, LUA_REGISTRYINDEX, size_cast<int>( ( intp )hScript ) );
	}

	// stack good
	ScriptStatus_t ExecuteFunction
	(
		HSCRIPT hFunction,
		ScriptVariant_t *pArgs,
		intp nArgs,
		ScriptVariant_t *pReturn,
		HSCRIPT hScope,
		bool bWait
	) override
	{
		if ( hScope == INVALID_HSCRIPT )
		{
			DevWarning( "ExecuteFunction: Invalid scope handed to script VM\n" );
			return SCRIPT_ERROR;
		}

#if 0
		if ( m_hDbg )
		{
			extern bool g_bSqDbgTerminateScript;
			if ( g_bSqDbgTerminateScript )
			{
				DisconnectDebugger();
				g_bSqDbgTerminateScript = false;
			}
		}
#endif

		Assert( bWait );

		try
		{
			if ( hFunction )
			{
				intp nStackSize = GetStackSize();

				lua_rawgeti( m_LuaState, LUA_REGISTRYINDEX, ( intp )hFunction );

				int nTopStack = lua_gettop( m_LuaState );

				if ( hScope )
				{
					lua_rawgeti( m_LuaState, LUA_REGISTRYINDEX, ( intp )hScope );
				}

				for ( int i = 0; i < nArgs; i++ )
				{
					PushVariant( m_LuaState, pArgs[i] );
				}

				lua_call( m_LuaState, lua_gettop( m_LuaState ) - nTopStack, ( pReturn ? 1 : 0 ) );
				if ( pReturn )
				{
					ConvertToVariant( -1, m_LuaState, pReturn );
				}

				lua_pop( m_LuaState, nStackSize - GetStackSize() );

				return SCRIPT_DONE;
			}

			if ( pReturn )
			{
				pReturn->m_type = FIELD_VOID;
			}
		}
		catch( const char *pszString )
		{
			FatalError( pszString );
		}

		return SCRIPT_ERROR;
	}

	// stack good
	static int TranslateCall( lua_State *pState )
	{
		int										nActualParams = lua_gettop( pState );
		ScriptFunctionBinding_t					*pVMScriptFunction = ( ScriptFunctionBinding_t * )lua_touserdata( pState, lua_upvalueindex( 1 ) );
		int										nFormalParams = pVMScriptFunction->m_desc.m_Parameters.Count();
		CUtlVectorFixed<ScriptVariant_t, 14>	params;
		ScriptVariant_t							returnValue;
		bool									bCallFree = false;

		params.SetSize( nFormalParams );

		void *pObject = nullptr;

		if ( nActualParams )
		{
			int nOffset = 1;

			if ( ( pVMScriptFunction->m_flags & SF_MEMBER_FUNC ) )
			{
				InstanceContext_t *pInstanceContext = ( InstanceContext_t * )lua_touserdata( pState, nOffset );
				pObject = pInstanceContext->pInstance;

				if ( pInstanceContext->pClassDesc->pHelper )
				{
					pObject = pInstanceContext->pClassDesc->pHelper->GetProxied( pObject );
				}

				if ( !pObject )
				{
//					sq_throwerror( hVM, "Accessed null instance" );
					return SCRIPT_ERROR;
				}

				nOffset++;
				nActualParams--;
			}

			int iLimit = ( nActualParams < nFormalParams ? nActualParams : nFormalParams );
			ScriptDataType_t *pCurParamType = pVMScriptFunction->m_desc.m_Parameters.Base();

			for ( int i = 0; i < iLimit; i++, pCurParamType++ )
			{
				switch ( *pCurParamType )
				{
					case FIELD_FLOAT:
						params[ i ] = ( float )lua_tonumber( pState, i + nOffset );
						break;

					case FIELD_CSTRING:
						params[ i ] = lua_tostring( pState, i + nOffset ); 
						break;

					case FIELD_VECTOR:
						{
							params[ i ] = lua_getvec3( pState, i + nOffset );
							break;
						}

					case FIELD_INTEGER:
						params[ i ] = size_cast<int>( lua_tonumber( pState, i + nOffset ) );
						break;

					case FIELD_BOOLEAN:
						params[ i ] = lua_toboolean( pState, i + nOffset ) != 0;
						break;

					case FIELD_CHARACTER:
						params[ i ] = lua_tostring( pState, i + nOffset )[0]; 
						break;

					case FIELD_HSCRIPT:
						{
							lua_pushvalue( pState,  i + nOffset );
							params[ i ] = ( HSCRIPT )( intp )luaL_ref( pState, LUA_REGISTRYINDEX );
//							params[ i ].m_flags |= SV_FREE;
							bCallFree = true;
							break;
						}

					default:
						break;
				}
			}
		}

		(*pVMScriptFunction->m_pfnBinding)( pVMScriptFunction->m_pFunction, pObject, params.Base(), params.Count(), ( pVMScriptFunction->m_desc.m_ReturnType != FIELD_VOID ) ? &returnValue : NULL );

		if ( pVMScriptFunction->m_desc.m_ReturnType != FIELD_VOID )
		{
			PushVariant( pState, returnValue );
		}

		if ( bCallFree )
		{
			for ( intp i = 0; i < params.Count(); i++ )
			{
				ReleaseVariant( pState, params[ i ] );
//				params[i].Free();
			}
		}

		return ( ( pVMScriptFunction->m_desc.m_ReturnType != FIELD_VOID ) ? 1 : 0 ) ;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	// stack good
	void RegisterFunctionGuts( ScriptFunctionBinding_t *pScriptFunction, HSCRIPT pOwningClass = NULL )
	{
		char szTypeMask[ 64 ];

		if ( pScriptFunction->m_desc.m_Parameters.Count() > ssize(szTypeMask) - 1 )
		{
			AssertMsg1( 0, "Too many agruments for script function %s\n", pScriptFunction->m_desc.m_pszFunction );
			return;
		}

		szTypeMask[0] = '.';
		char *pCurrent = &szTypeMask[1];
		for ( intp i = 0; i < pScriptFunction->m_desc.m_Parameters.Count(); i++, pCurrent++ )
		{
			switch ( pScriptFunction->m_desc.m_Parameters[i] )
			{
				case FIELD_CSTRING:
					*pCurrent = 's';
					break;
				case FIELD_FLOAT:
				case FIELD_INTEGER:
					*pCurrent = 'n';
					break;
				case FIELD_BOOLEAN:
					*pCurrent = 'b';
					break;

				case FIELD_VECTOR:
					*pCurrent = 'x';
					break;

				case FIELD_HSCRIPT:
					*pCurrent = '.';
					break;

				case FIELD_CHARACTER:
				default:
					*pCurrent = FIELD_VOID;
					AssertMsg( 0 , "Not supported" );
					break;
			}
		}

		int nStackIndex = -1;
		if ( pOwningClass )
		{
			lua_rawgeti( m_LuaState, LUA_REGISTRYINDEX, ( intp )pOwningClass );
			if ( !lua_isnil( m_LuaState, -1 ) )
			{
				nStackIndex = lua_gettop( m_LuaState );
			}
		}

		Assert( pCurrent - szTypeMask < ssize(szTypeMask) - 1 );
		*pCurrent = 0;
		lua_pushstring( m_LuaState, pScriptFunction->m_desc.m_pszScriptName );
		lua_pushlightuserdata( m_LuaState, pScriptFunction );
		lua_pushcclosure( m_LuaState, &TranslateCall, 1 );

		if ( nStackIndex == -1 )
		{
			lua_setglobal( m_LuaState, pScriptFunction->m_desc.m_pszScriptName );
		}

		if ( pOwningClass )
		{
			lua_pop( m_LuaState, 1 );
		}

		if ( nStackIndex == -1 )
		{
			Msg( "VLua: Registered GLOBAL function %s\n", pScriptFunction->m_desc.m_pszScriptName );
		}
		else
		{
			Msg( "VLua: Registered TABLE function %s\n", pScriptFunction->m_desc.m_pszScriptName );
		}

#if 0
		if ( developer.GetInt() )
		{
			const char *pszHide = SCRIPT_HIDE;
			if ( !pScriptFunction->m_desc.m_pszDescription || *pScriptFunction->m_desc.m_pszDescription != *pszHide )
			{
				std::string name;
				std::string signature;

				if ( pClassDesc )
				{
					name += pClassDesc->m_pszScriptName;
					name += "::";
				}

				name += pScriptFunction->m_desc.m_pszScriptName;

				signature += FieldTypeToString( pScriptFunction->m_desc.m_ReturnType );
				signature += ' ';
				signature += name;
				signature += '(';
				for ( int i = 0; i < pScriptFunction->m_desc.m_Parameters.Count(); i++ )
				{
					if ( i != 0 )
					{
						signature += ", ";
					}

					signature+= FieldTypeToString( pScriptFunction->m_desc.m_Parameters[i] );
				}
				signature += ')';

				sq_pushobject( m_hVM, LookupObject( "RegisterFunctionDocumentation", NULL, false ) );
				sq_pushroottable( m_hVM );
				sq_pushobject( m_hVM, hFunction );
				sq_pushstring( m_hVM, name.c_str(), name.length() );
				sq_pushstring( m_hVM, signature.c_str(), signature.length() );
				sq_pushstring( m_hVM, pScriptFunction->m_desc.m_pszDescription, -1 );
				sq_call( m_hVM, 5, false, /*false*/ true );
				sq_pop( m_hVM, 1 );
			}
		}
#endif
	}

	void RegisterFunction( ScriptFunctionBinding_t *pScriptFunction ) override
	{
		RegisterFunctionGuts( pScriptFunction );
	}

	static int custom_index( lua_State *pState )
	{
		InstanceContext_t *pInstanceContext = ( ( InstanceContext_t * )lua_touserdata( pState, 1 ) );
		ScriptClassDesc_t *pVMScriptFunction = pInstanceContext->pClassDesc;
		const char	*pszKey = luaL_checkstring( pState, 2 );

		while( pVMScriptFunction )
		{
			lua_getfield( pState, LUA_REGISTRYINDEX, pVMScriptFunction->m_pszClassname );
			if ( lua_isnil( pState, -1 ) )
			{
				break;
			}
			lua_pushstring( pState, pszKey );
			lua_rawget( pState, -2 );

			if ( lua_isnil( pState, -1 ) )
			{
				lua_pop( pState, 2 );
				pVMScriptFunction = pVMScriptFunction->m_pBaseDesc;
			}
			else
			{
				break;
			}
		}

		return 1;
	}

	// stack good
	bool RegisterClass( ScriptClassDesc_t *pClassDesc ) override
	{
		if ( luaL_newmetatable( m_LuaState, pClassDesc->m_pszScriptName ) == 0 )
		{
			lua_pop( m_LuaState, 1 );
			return true;
		}

//		lua_pushcclosure( m_LuaState, custom_gc, 0 );
//		lua_setfield( m_LuaState, -2, "__gc" );
		lua_pushcclosure( m_LuaState, custom_index, 0 );
		lua_setfield( m_LuaState, -2, "__index" );

		HSCRIPT ClassReference = ( HSCRIPT )( intp )luaL_ref( m_LuaState, LUA_REGISTRYINDEX );

		for ( auto &b : pClassDesc->m_FunctionBindings )
		{
			RegisterFunctionGuts( &b, ClassReference );
		}

		if ( pClassDesc->m_pBaseDesc )
		{
			RegisterClass( pClassDesc->m_pBaseDesc );
		}

		luaL_unref( m_LuaState, LUA_REGISTRYINDEX, size_cast<int>( ( intp )ClassReference ) );

		return true;
	}


	// stack good
	HSCRIPT RegisterInstance( ScriptClassDesc_t *pClassDesc, void *pInstance ) override
	{
		if ( !RegisterClass( pClassDesc ) )
		{
			return NULL;
		}

		InstanceContext_t *pInstanceContext = ( InstanceContext_t * )lua_newuserdata( m_LuaState, sizeof( InstanceContext_t ) );
		pInstanceContext->pInstance = pInstance;
		pInstanceContext->pClassDesc = pClassDesc;
		luaL_getmetatable( m_LuaState, pClassDesc->m_pszScriptName );
		lua_setmetatable( m_LuaState, -2 );

		HSCRIPT Instance = ( HSCRIPT )( intp )luaL_ref( m_LuaState, LUA_REGISTRYINDEX );

		return Instance;
	}

	// stack good
	void SetInstanceUniqeId( HSCRIPT hInstance, const char *pszId ) override
	{
#if 0
		TValue	*pValue = ( TValue * )hInstance;
		if ( !hInstance )
		{
			Assert( 0 );
			return;
		}

		Assert( ttislightuserdata( pValue ) );
		InstanceContext_t *pInstanceContext = ( InstanceContext_t * )( pValue->value.p );
		if ( pInstanceContext == NULL )
		{
			Assert( 0 );
			return;
		}

		lua_getfield( m_LuaState, LUA_REGISTRYINDEX, pInstanceContext->szName );	//--
		strcpy( pInstanceContext->szName, pszId );
		lua_setfield( m_LuaState, LUA_REGISTRYINDEX, pInstanceContext->szName );	//--
#endif
		Assert( 0 );
	}

	// stack good
	void RemoveInstance( HSCRIPT hInstance ) override
	{
#if 0
		TValue	*pValue = ( TValue * )hInstance;
		if ( !pValue )
		{
			Assert( 0 );
			return;
		}

		Assert( ttislightuserdata( pValue ) );
		InstanceContext_t *pInstanceContext = ( InstanceContext_t * )( pValue->value.p );

		lua_pushnil( m_LuaState );
		lua_setfield( m_LuaState, LUA_REGISTRYINDEX, pInstanceContext->szName );
//		lua_pushnil( m_LuaState );	//--
//		lua_setglobal( m_LuaState, pInstanceContext->szName );	//--

		delete pInstanceContext;
		delete pValue;
#endif
		Assert( 0 );
	}

	void *GetInstanceValue( HSCRIPT hInstance, ScriptClassDesc_t *pExpectedType = NULL ) override
	{
		Assert( 0 );
		return nullptr;
	}

	bool GenerateUniqueKey( const char *pszRoot, char *pBuf, int nBufSize ) override
	{
		Assert( V_strlen(pszRoot) + 32 <= nBufSize );
		Q_snprintf( pBuf, nBufSize, "_%x%I64x_%s", RandomInt(0, 0xfff), m_iUniqueIdSerialNumber++, pszRoot ); // random to limit key compare when serial number gets large
		return true;
	}

	// stack good
	bool ValueExists( HSCRIPT hScope, const char *pszKey ) override
	{
		Assert( hScope == nullptr );

		lua_getglobal( m_LuaState, pszKey );
		bool bResult = ( lua_isnil( m_LuaState, -1 ) == false );

		lua_pop( m_LuaState, 1 );

		return bResult;
	}

	bool SetValue( HSCRIPT hScope, const char *pszKey, const char *pszValue ) override
	{
		// Not supported yet.
		Assert(0);
		return false;
	}

	// stack good
	bool SetValue( HSCRIPT hScope, const char *pszKey, const ScriptVariant_t &value ) override
	{
		if ( hScope )
		{
//			Msg( "SetValue: SCOPE %s\n", pszKey );
			lua_rawgeti( m_LuaState, LUA_REGISTRYINDEX, ( intp )hScope );
			lua_pushstring( m_LuaState, pszKey );
			PushVariant( m_LuaState, value );
			lua_settable( m_LuaState, -3 );

			lua_pop( m_LuaState, 1 );
		}
		else
		{
//			Msg( "SetValue: NORMAL %s\n", pszKey );
			lua_pushstring( m_LuaState, pszKey );
			PushVariant( m_LuaState, value );
			lua_setglobal( m_LuaState, pszKey );
		}

		return true;
	}

	virtual bool SetValue( HSCRIPT hScope, int nIndex, const ScriptVariant_t &value )
	{
		Assert( hScope );

		lua_rawgeti( m_LuaState, LUA_REGISTRYINDEX, ( intp )hScope );
		lua_pushinteger( m_LuaState, nIndex );
		PushVariant( m_LuaState, value );
		lua_settable( m_LuaState, -3 );

		return true;
	}

	void CreateTable( ScriptVariant_t &Result ) override
	{
		lua_newtable( m_LuaState );
		ConvertToVariant( -1, m_LuaState, &Result );
	}

	// stack good
	int	GetNumTableEntries( HSCRIPT hScope ) override
	{
		// Should this also check for 0?
		if ( hScope == INVALID_HSCRIPT )
			return 0;

		int nCount = 0;

		lua_rawgeti( m_LuaState, LUA_REGISTRYINDEX, ( intp )hScope );
//		int count = lua_objlen( m_LuaState, -1 );

		lua_pushnil( m_LuaState ); /* first key */
		while ( lua_next( m_LuaState, -2 ) != 0 ) 
		{
			/* key is at index -2 and value at index -1 */
//			Msg ("%s - %s\n", lua_typename(m_LuaState, lua_type(m_LuaState, -2)), lua_typename(m_LuaState, lua_type(m_LuaState, -1)));
			nCount++;
			lua_pop( m_LuaState, 1 ); /* removes value; keeps key for next iteration */
		}

		lua_pop( m_LuaState, 1 ); /* removes value; keeps key for next iteration */

		return nCount;
	}

	// stack good
	int GetKeyValue( HSCRIPT hScope, int nIterator, ScriptVariant_t *pKey, ScriptVariant_t *pValue ) override
	{
		int nCount = 0;
		intp nStackSize = GetStackSize();

		lua_rawgeti( m_LuaState, LUA_REGISTRYINDEX, ( intp )hScope );
		//		int count = lua_objlen( m_LuaState, -1 );

		lua_pushnil( m_LuaState ); /* first key */
		while ( lua_next( m_LuaState, -2 ) != 0 && nCount < nIterator )
		{
			nCount++;
			lua_pop( m_LuaState, 1 ); /* removes value; keeps key for next iteration */
		}

//		Msg ("%s - %s\n", lua_typename(m_LuaState, lua_type(m_LuaState, -2)), lua_typename(m_LuaState, lua_type(m_LuaState, -1)));

		ConvertToVariant( -2, m_LuaState, pKey );
		ConvertToVariant( -1, m_LuaState, pValue );

		lua_pop( m_LuaState, 3 ); /* removes value; keeps key for next iteration */

		lua_pop( m_LuaState, nStackSize - GetStackSize() );

		return nCount + 1;
	}

	bool GetValue( HSCRIPT hScope, const char *pszKey, ScriptVariant_t *pValue ) override
	{
		intp nStackSize = GetStackSize();

		if ( hScope )
		{
			lua_rawgeti( m_LuaState, LUA_REGISTRYINDEX, ( intp )hScope );
			lua_getfield( m_LuaState, -1, pszKey );
			if ( lua_isnil( m_LuaState, -1 ) )
			{
				lua_pop( m_LuaState, nStackSize - GetStackSize() );
				return false;
			}
		}
		else
		{
			lua_getglobal( m_LuaState, pszKey );
			if ( lua_isnil( m_LuaState, -1 ) )
			{
				lua_pop( m_LuaState, nStackSize - GetStackSize() );
				return false;
			}
		}

		ConvertToVariant( -1, m_LuaState, pValue );
		lua_pop( m_LuaState, nStackSize - GetStackSize() );

		return true;
	}

	virtual bool GetValue( HSCRIPT hScope, int nIndex, ScriptVariant_t *pValue )
	{
		if ( hScope )
		{
			intp nStackSize = GetStackSize();

			lua_rawgeti( m_LuaState, LUA_REGISTRYINDEX, ( intp )hScope );
			lua_rawgeti( m_LuaState, -1, nIndex );
			if ( lua_isnil( m_LuaState, -1 ) )
			{
				lua_pop( m_LuaState, nStackSize - GetStackSize() );
				return false;
			}

			ConvertToVariant( -1, m_LuaState, pValue );
			lua_pop( m_LuaState, nStackSize - GetStackSize() );

			return true;
		}

		return false;
	}

	void ReleaseValue( ScriptVariant_t &value ) override
	{
		ReleaseVariant( m_LuaState, value );
	}

	bool ClearValue( HSCRIPT hScope, const char *pszKey ) override
	{
		Assert( 0 );
		return false;
	}

	void WriteState( CUtlBuffer *pBuffer ) override
	{
		Assert( 0 );
	}

	void ReadState( CUtlBuffer *pBuffer ) override
	{
		Assert( 0 );
	}

	void RemoveOrphanInstances() override
	{
		Assert( 0 );
	}

	void DumpState() override
	{
		Assert( 0 );
	}

	bool RaiseException( const char *pszExceptionText ) override
	{
		return true;
	}

	void SetOutputCallback( ScriptOutputFunc_t pFunc ) override
	{
		m_OutputFunc = pFunc;

		lua_pushstring( m_LuaState, "print" );
		ScriptOutputFunc_t *pOutputCallback = static_cast<ScriptOutputFunc_t *>
		(
			lua_newuserdata( m_LuaState, sizeof( ScriptOutputFunc_t ) )
		);
		*pOutputCallback = m_OutputFunc;

		lua_pushcclosure( m_LuaState, PrintFunc, 1 );
		lua_setglobal( m_LuaState, "print" );
	}

	void SetErrorCallback( ScriptErrorFunc_t pFunc ) override
	{
		m_ErrorFunc = pFunc;
	}
};


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

IScriptVM *ScriptCreateLuaVM()
{
	return new CLuaVM;
}

void ScriptDestroyLuaVM( IScriptVM *pVM )
{
	CLuaVM *pLuaVM = assert_cast< CLuaVM * >( pVM );
	delete pLuaVM;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#ifdef VLUA_TEST
CLuaVM g_LuaVM;
IScriptVM *g_pScriptVM = &g_LuaVM;

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

#include <ctime>
#include <conio.h>

#include "posix_file_stream.h"
#include "tier0/fasttimer.h"

static void FromScript_AddBehavior( const char *pBehaviorName, HSCRIPT hTable )
{
	ScriptVariant_t		KeyVariant, ValueVariant;

	Msg( "Behavior: %s\n", pBehaviorName );

	int nInterator = 0;
	int index =	g_pScriptVM->GetNumTableEntries( hTable );
	for( int i = 0; i < index; i++ )
	{
		nInterator = g_pScriptVM->GetKeyValue( hTable, nInterator, &KeyVariant, &ValueVariant );

		Msg( "   %d: %s / %s\n", i, KeyVariant.m_pszString, ValueVariant.m_pszString );

		g_pScriptVM->ReleaseValue( KeyVariant );
		g_pScriptVM->ReleaseValue( ValueVariant );
	}
}

static Vector MyVectorAdd( Vector A, Vector B )
{
	return A + B;
}

void TestOutput( const char *pszText )
{
	Msg( "%s\n", pszText );
}

bool TestError( ScriptErrorLevel_t level, const char *pszText )
{
	if ( level == SCRIPT_LEVEL_ERROR )
		Error( "%s\n", pszText );
	else if ( level == SCRIPT_LEVEL_WARNING )
		Warning( "%s\n", pszText );
	else
		Msg( "%s\n", pszText );

	return true;
}


class CMyClass
{
public:
	bool Foo( int );
	void Bar( HSCRIPT TableA, HSCRIPT TableB );
	float FooBar( int, const char * );
	float OverlyTechnicalName( bool );
};

bool CMyClass::Foo( int test  )
{
	return true;
}

void CMyClass::Bar( HSCRIPT TableA, HSCRIPT TableB )
{
	ScriptVariant_t MyValue;

//	g_pScriptVM->CreateTable( MyTable );

	MyValue = 10;
	g_pScriptVM->SetValue( TableA, "1", MyValue );
	MyValue = 20;
	g_pScriptVM->SetValue( TableA, "2", MyValue );
	MyValue = 30;
	g_pScriptVM->SetValue( TableA, "3", MyValue );

	MyValue = 100;
	g_pScriptVM->SetValue( TableB, "1", MyValue );
	MyValue = 200;
	g_pScriptVM->SetValue( TableB, "2", MyValue );
	MyValue = 300;
	g_pScriptVM->SetValue( TableB, "3", MyValue );

//	return MyTable;
}

float CMyClass::FooBar( int test1, const char *test2 )
{
	return 2.34f;
}

float CMyClass::OverlyTechnicalName( bool test )
{
	return 4.56f;
}

BEGIN_SCRIPTDESC_ROOT_NAMED( CMyClass , "CMyClass", SCRIPT_SINGLETON "" )
	DEFINE_SCRIPTFUNC( Foo, "" )
	DEFINE_SCRIPTFUNC( Bar, "" )
	DEFINE_SCRIPTFUNC( FooBar, "" )
	DEFINE_SCRIPTFUNC_NAMED( OverlyTechnicalName, "SimpleMemberName", "" )
END_SCRIPTDESC();

class CMyDerivedClass : public CMyClass
{
public:
	float DerivedFunc() const;
};

BEGIN_SCRIPTDESC( CMyDerivedClass, CMyClass, SCRIPT_SINGLETON "" )
	DEFINE_SCRIPTFUNC( DerivedFunc, "" )
END_SCRIPTDESC();

float CMyDerivedClass::DerivedFunc() const
{
	return 8.91f;
}

CMyDerivedClass derivedInstance;

void AnotherFunction()
{
	// Manual class exposure
	g_pScriptVM->RegisterClass( GetScriptDescForClass( CMyClass ) );

	// Auto registration by instance
	g_pScriptVM->RegisterInstance( &derivedInstance, "theInstance" );
}

int main( int argc, const char **argv)
{
	if ( argc < 2 )
	{
		fprintf( stderr, "No script specified.\n" );
		return EINVAL;
	}

	g_pScriptVM->Init();

	g_pScriptVM->SetOutputCallback( TestOutput );
	g_pScriptVM->SetErrorCallback( TestError );

	AnotherFunction();

	CCycleCount count;
	count.Sample();
	RandomSeed( time( NULL ) ^ count.GetMicroseconds() );
	ScriptRegisterFunction( g_pScriptVM, RandomFloat, "" );
	ScriptRegisterFunction( g_pScriptVM, RandomInt, "" );

	ScriptRegisterFunction( g_pScriptVM, FromScript_AddBehavior, "" );
	ScriptRegisterFunction( g_pScriptVM, MyVectorAdd, "" );
	
	if ( argc == 3 && *argv[2] == 'd' )
	{
		g_pScriptVM->ConnectDebugger();
	}

	const char *pszScript = argv[1];
	const char *scriptName{ strrchr( pszScript, '\\' ) };

	scriptName = scriptName ? scriptName + 1 : pszScript;

	int key;
	CScriptScope scope;
	scope.Init( "TestScope" );
	do 
	{
		auto [hFile, rc] = se::posix::posix_file_stream_factory::open( pszScript, "rb" );
		if ( rc )
		{
			fprintf( stderr, "Can't open \"%s\" (%d): %s.\n",
				pszScript, rc.value(), rc.message().c_str() );
			return rc.value();
		}

		int64_t nFileLen;
		std::tie(nFileLen, rc) = hFile.size();
		if ( rc )
		{
			fprintf( stderr, "Can't get \"%s\" size (%d): %s.\n",
				pszScript, rc.value(), rc.message().c_str() );
			return rc.value();
		}
		if ( nFileLen > std::numeric_limits<unsigned>::max() )
		{
			fprintf( stderr, "File \"%s\" is too large.\n",	pszScript );
			return EINVAL;
		}

		std::unique_ptr<char[]> pBuf = std::make_unique<char[]>(nFileLen + 1);
		std::tie(std::ignore, rc) = hFile.read( pBuf.get(), nFileLen, 1, nFileLen );
		pBuf[nFileLen] = '\0';

		if (1)
		{
			printf( "Executing script \"%s\"\n----------------------------------------\n", pszScript );
			HSCRIPT hScript = g_pScriptVM->CompileScript( pBuf.get(), scriptName );
			if ( hScript )
			{
				if ( scope.Run( hScript ) != SCRIPT_ERROR )
				{
					printf( "----------------------------------------\n" );
					printf("Script complete.  Press q to exit, m to dump memory usage, enter to run again.\n");
				}
				else
				{
					fprintf( stderr, "----------------------------------------\n" );
					fprintf( stderr, "Script execution error.  Press q to exit, m to dump memory usage, enter to run again.\n");
				}
				g_pScriptVM->ReleaseScript( hScript );
			}
			else
			{
				fprintf( stderr, "----------------------------------------\n" );
				fprintf( stderr, "Script failed to compile.  Press q to exit, m to dump memory usage, enter to run again.\n");
			}
		}

 		key = _getch(); // Keypress before exit
		if ( key == 'm' )
		{
			Msg( "%d\n", g_pMemAlloc->GetSize( nullptr ) );
		}
	} while ( key != 'q' );

	scope.Term();
	g_pScriptVM->DisconnectDebugger();

	g_pScriptVM->Shutdown();
	return 0;
}

#endif


// add a check stack auto class to each function

