//========== Copyright (c) Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//==========================================================================

#if defined( POSIX )
#include <ctype.h>
#include <wctype.h>
#endif

// dimhotepus: Allow exceptions.
// #define _HAS_EXCEPTIONS 0

#include "vsq_vec3.h"

#include "datamap.h"
#include "tier0/platform.h"
#include "tier0/vprof.h"
#include "tier1/utlmap.h"
#include "tier1/functors.h"
#include "tier1/utlvector.h"
#include "tier1/utlhash.h"
#include "tier1/utlbuffer.h"
#include "tier1/fmtstr.h"
#include "tier1/convar.h"
#include "mathlib/vector.h"
#include "vstdlib/random.h"

#include "squirrel.h"
#include "sqstdaux.h"
#include "sqstdstring.h"
#include "sqstdmath.h"
#include "sqplus.h"
#include "sqrdbg.h"
#include "../squirrel/sqstate.h"
#include "../squirrel/sqvm.h"
#include "../squirrel/sqobject.h"
#include "../squirrel/sqstring.h"
#include "../squirrel/sqarray.h"
#include "../squirrel/sqtable.h"
#include "../squirrel/squserdata.h"
#include "../squirrel/sqfuncproto.h"
#include "../squirrel/sqclass.h"
#include "../squirrel/sqclosure.h"
#include "sqdbgserver.h"

#include "vscript/ivscript.h"

#include "init_nut.h"

#include "tier0/memdbgon.h"

#ifdef VSQUIRREL_DEBUG_SERIALIZATION
static SQObjectType lastType;
#endif

//-----------------------------------------------------------------------------
// Stub out unwanted features
//-----------------------------------------------------------------------------
extern "C" 
{

SQRESULT sqstd_register_iolib(HSQUIRRELVM)
{
	return SQ_OK;
}

SQRESULT sqstd_loadfile(HSQUIRRELVM,const SQChar *,SQBool)
{
	return SQ_ERROR;
}

}

//-------------------------------------------------------------------------
// Helpers
//-------------------------------------------------------------------------

static constexpr const char *FieldTypeToString( int type )
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

static constexpr const char *SQTypeToString( SQObjectType sqType )
{
	switch( sqType )
	{
		case OT_NULL:			return "NULL";
		case OT_INTEGER:		return "INTEGER";
		case OT_FLOAT:			return "FLOAT";
		case OT_BOOL:			return "BOOL";
		case OT_STRING:			return "STRING";
		case OT_TABLE:			return "TABLE";
		case OT_ARRAY:			return "ARRAY";
		case OT_USERDATA:		return "USERDATA";
		case OT_CLOSURE:		return "CLOSURE";
		case OT_NATIVECLOSURE:	return "NATIVECLOSURE";
		case OT_GENERATOR:		return "GENERATOR";
		case OT_USERPOINTER:	return "USERPOINTER";
		case OT_THREAD:			return "THREAD";
		case OT_FUNCPROTO:		return "FUNCPROTO";
		case OT_CLASS:			return "CLASS";
		case OT_INSTANCE:		return "INSTANCE";
		case OT_WEAKREF:		return "WEAKREF";
		case OT_OUTER:			return "OUTER";
	}
	return "<unknown>";
}


//-----------------------------------------------------------------------------
// Bridge code, some cribbed from SqPlus
//-----------------------------------------------------------------------------

const HSQOBJECT INVALID_HSQOBJECT = { (SQObjectType)-1, (SQTable *)-1 };
const HSQOBJECT NULL_HSQOBJECT = { SQObjectType::OT_NULL, nullptr };

inline bool operator==( const HSQOBJECT &lhs, const HSQOBJECT &rhs ) { return ( lhs._type == rhs._type && lhs._unVal.pTable == rhs._unVal.pTable ); }
inline bool operator!=( const HSQOBJECT &lhs, const HSQOBJECT &rhs ) { return !operator==( lhs, rhs ); }

class CSquirrelVM final : public IScriptVM
{
public:
	explicit CSquirrelVM( HSQUIRRELVM hVM = nullptr )
	  :	m_hVM( hVM ), m_hDbg( nullptr ), m_PtrMap( DefLessFunc(void *) ), m_iUniqueIdSerialNumber( 0 )
#ifndef VSQUIRREL_TEST
	    , developer( "developer" )
#else
	    , developer( "developer", "1" )
#endif
	{
		m_hOnCreateScopeFunc = NULL_HSQOBJECT;
		m_hOnReleaseScopeFunc = NULL_HSQOBJECT;
		m_hClassVector = NULL_HSQOBJECT;
		m_ErrorString = NULL_HSQOBJECT;
		m_TimeStartExecute = 0.0;
		m_pBuffer = nullptr;
	}

	bool Init() override
	{
		constexpr SQInteger stackSize{1024};

		m_hVM = sq_open(stackSize);
		if ( !m_hVM )
		{
			DWarning( "vscript:squirrel",
				0,
				"Unable to create %s VM with 0x%x stack bytes. Out of memory?\n",
				SQUIRREL_VERSION, stackSize );
			return false;
		}

		_ss( m_hVM )->_foreignptr = this;

		// Display script compile errors.
		sq_setcompilererrorhandler(m_hVM, &CompileErrorHandler);
		// sq_notifyallexceptions(m_hVM, _debug_script_level > 5);
		// Dump output.
		sq_setprintfunc(m_hVM, &PrintHandler, &ErrorHandler);
		// Display runtime errors to client.
		sq_newclosure(m_hVM, &RuntimeErrorHandler, 0);
		sq_seterrorhandler(m_hVM);


		sq_pushroottable(m_hVM);
		if (SQ_FAILED(sqstd_register_mathlib(m_hVM)))
		{
			sq_pop(m_hVM, 1);
			DWarning( "vscript:squirrel",
				0,
				"Unable to register math library for %s VM.\n",
				SQUIRREL_VERSION );
			return false;
		}
		if (SQ_FAILED(sqstd_register_stringlib(m_hVM)))
		{
			sq_pop(m_hVM, 1);
			DWarning( "vscript:squirrel",
				0,
				"Unable to register string library for %s VM.\n",
				SQUIRREL_VERSION );
			return false;
		}
		sq_pop(m_hVM, 1);


		if ( IsDebug() || developer.GetInt() > 0 )
		{
			sq_enabledebuginfo( m_hVM, SQTrue );

			DMsg( "vscript:squirrel",
				0,
				"Enabled debug info due to "
#ifdef _DEBUG
				"DEBUG build"
#else
				"developer convar set to non-zero"
#endif
				".\n" );
		}

		sq_pushroottable( m_hVM );

		sq_pushstring( m_hVM, "developer", -1 );
		sq_newclosure( m_hVM, &GetDeveloper, 0 );
		if (SQ_FAILED(sq_setnativeclosurename(m_hVM, -1, "developer")))
		{
			sq_pop(m_hVM, 1);
			DWarning( "vscript:squirrel",
				0,
				"Unable to set native 'developer' closure name %s VM.\n",
				SQUIRREL_VERSION );
			return false;
		}
		sq_createslot( m_hVM, -3 );

		sq_pushstring( m_hVM, "GetFunctionSignature", -1 );
		sq_newclosure( m_hVM, &GetFunctionSignature, 0 );
		if (SQ_FAILED(sq_setnativeclosurename(m_hVM, -1, "GetFunctionSignature")))
		{
			sq_pop(m_hVM, 1);
			DWarning( "vscript:squirrel",
				0,
				"Unable to set native 'GetFunctionSignature' closure name %s VM.\n",
				SQUIRREL_VERSION );
			return false;
		}
		sq_createslot( m_hVM, -3 );

		sq_pop( m_hVM, 1 );


		m_TypeMap.Init( 256 );

		if (SQ_FAILED(vsq_openvec3(m_hVM, &m_hClassVector)))
		{
			return false;
		}

		// Initialization scripts & hookup
		Run( (const char *)g_Script_init, "init.nut" );

		m_hOnCreateScopeFunc = LookupObject( "VSquirrel_OnCreateScope" );
		m_hOnReleaseScopeFunc = LookupObject( "VSquirrel_OnReleaseScope" );

		DMsg( "vscript:squirrel", 0, "Created %s VM.\n", SQUIRREL_VERSION );

		return true;
	}

	bool Frame( float simTime ) override
	{
		// <sergiy> removed garbage collection that was called at least 2
		// times a frame (60 fps server tick / 30fps game = 2 calls a frame).
		//
		// It's not necessary because our scripts are supposed to never create
		// circular references and everything else is handled with ref counting.
		//
		// For the case of bugs creating circular references, the plan is to add
		// diagnostics that detects such loops and warns the developer.
		if ( m_hDbg )
		{
			if (SQ_FAILED(sq_rdbg_update( m_hDbg )))
			{
				DWarning( "vscript:squirrel", 0, "Remote debugger lost connection...\n" );
			}

			if ( !m_hDbg->IsConnected() )
				DisconnectDebugger();
		}

		return false;
	}

	void Shutdown() override
	{
		if ( m_hVM )
		{
			DisconnectDebugger();

			sq_collectgarbage( m_hVM );
			sq_pushnull( m_hVM );
			sq_setroottable( m_hVM );

			_ss( m_hVM )->_foreignptr = nullptr;

			sq_close( m_hVM );
			m_hVM = nullptr;

			DMsg( "vscript:squirrel", 0, "Shutdown %s VM.\n", SQUIRREL_VERSION );
		}

		m_TypeMap.Purge();
	}

	ScriptLanguage_t GetLanguage() override
	{
		return SL_SQUIRREL;
	}

	const char *GetLanguageName() override
	{
		return "Squirrel";
	}

	void AddSearchPath( const char *pszSearchPath ) override
	{
	}

	HSQUIRRELVM GetVM()
	{
		return m_hVM;
	}

	bool ConnectDebugger() override
	{
		if ( developer.GetInt() > 0 )
		{
			constexpr unsigned short port{ 1234 };

			if ( !m_hDbg )
			{
				m_hDbg = sq::dbg::sq_rdbg_init( m_hVM, port, SQTrue );
			}

			if ( !m_hDbg )
			{
				return false;
			}

			DMsg( "vscript:squirrel", 0, "Waiting for remote debugger to connect on localhost:%hu....", port );

			//!! SUSPENDS THE APP UNTIL THE DEBUGGER CLIENT CONNECTS
			return SQ_SUCCEEDED( sq_rdbg_waitforconnections(m_hDbg) );
		}
		return false;
	}

	void DisconnectDebugger() override
	{
		if ( m_hDbg )
		{
			sq::dbg::sq_rdbg_shutdown( &m_hDbg );
			m_hDbg = nullptr;
		}
	}

	ScriptStatus_t Run( const char *pszScript, bool bWait = true ) override
	{
		Assert( bWait );

		const SQInteger rc = sq_compilebuffer(m_hVM,
			pszScript,(int)V_strlen(pszScript)*sizeof(SQChar),"unnamed",1);
		if (SQ_SUCCEEDED(rc))
		{
			HSQOBJECT hScript;
			sq_getstackobj(m_hVM,-1, &hScript);
			sq_addref(m_hVM, &hScript );
			sq_pop(m_hVM,1);

			ScriptStatus_t result = CSquirrelVM::ExecuteFunction( (HSCRIPT)(&hScript), NULL, 0, NULL, NULL, bWait );

			sq_release( m_hVM, &hScript );

			return result;
		}
		return SCRIPT_ERROR;
	}

	HSCRIPT CompileScript( const char *pszScript, const char *pszId = nullptr ) override
	{
		if ( !pszScript || !*pszScript )
		{
			return nullptr;
		}

		const SQInteger rc = sq_compilebuffer(m_hVM,
			pszScript,(int)V_strlen(pszScript)*sizeof(SQChar),(pszId) ? pszId : "unnamed",1);
		if (SQ_SUCCEEDED(rc)) 
		{
			auto *pRet = new HSQOBJECT;
			sq_getstackobj(m_hVM,-1,pRet);
			sq_addref(m_hVM, pRet);
			sq_pop(m_hVM,1);
			return (HSCRIPT)pRet;
		}
		return nullptr;
	}

	void ReleaseScript( HSCRIPT hScript ) override
	{
		ReleaseScriptObject( hScript );
	}

	ScriptStatus_t Run( HSCRIPT hScript, HSCRIPT hScope = nullptr, bool bWait = true ) override
	{
		return CSquirrelVM::ExecuteFunction( hScript, NULL, 0, NULL, hScope, bWait );
	}

	ScriptStatus_t Run( HSCRIPT hScript, bool bWait ) override
	{
		Assert( bWait );
		return CSquirrelVM::Run( hScript, (HSCRIPT)nullptr, bWait );
	}

	HSCRIPT CreateScope( const char *pszScope, HSCRIPT hParent = nullptr ) override
	{
		if ( !hParent )
		{
			hParent = (HSCRIPT)&m_hVM->_roottable;
		}

		HSQOBJECT result;
		sq_pushobject( m_hVM, m_hOnCreateScopeFunc );
		sq_pushroottable( m_hVM );
		sq_pushstring( m_hVM, pszScope, -1 );
		sq_pushobject( m_hVM, *((HSQOBJECT *)hParent) );
		if ( sq_call( m_hVM, 3, true, SQ_CALL_RAISE_ERROR ) == SQ_OK )
		{
			sq_getstackobj(m_hVM,-1,&result);
			sq_pop(m_hVM,2);
		}
		else
		{
			result = NULL_HSQOBJECT;
			sq_pop(m_hVM,1);
		}

		if ( sq_isnull( result ) )
		{
			return nullptr;
		}

		sq_addref(m_hVM, &result);
		HSQOBJECT *pRet = new HSQOBJECT;
		*pRet = result;
		return (HSCRIPT)pRet;
	}

	HSCRIPT ReferenceScope( HSCRIPT hScript ) override
	{
		if ( hScript == nullptr ) return nullptr;

		HSQOBJECT *pObj = reinterpret_cast<HSQOBJECT *>( hScript );
		if ( sq_isnull( *pObj ) )
		{
			return nullptr;
		}

		sq_addref(m_hVM, pObj);
		HSQOBJECT *pRet = new HSQOBJECT;
		*pRet = *pObj;
		return (HSCRIPT)pRet;
	}

	void ReleaseScope( HSCRIPT hScript ) override
	{
		HSQOBJECT &o = *((HSQOBJECT *)hScript);
		sq_pushobject( m_hVM, m_hOnReleaseScopeFunc );
		sq_pushroottable( m_hVM );
		sq_pushobject(m_hVM, o );
		sq_call( m_hVM, 2, false, SQ_CALL_RAISE_ERROR );
		sq_pop(m_hVM,1);

		ReleaseScriptObject( hScript );
	}

	HSQOBJECT LookupObject( const char *pszObject, HSCRIPT hScope = nullptr, bool bAddRef = true )
	{
		HSQOBJECT result = { OT_NULL, NULL };
		if ( !hScope )
		{
			sq_pushroottable( m_hVM );
		}
		else
		{
			if ( hScope == INVALID_HSCRIPT || *((HSQOBJECT *)hScope) == INVALID_HSQOBJECT || !sq_istable( *((HSQOBJECT *)hScope) ) )
			{
				return NULL_HSQOBJECT;
			}
			sq_pushobject( m_hVM, *((HSQOBJECT *)hScope) );
		}
		sq_pushstring( m_hVM, pszObject, -1 );
		if ( sq_get( m_hVM, -2 ) == SQ_OK )
		{
			sq_getstackobj(m_hVM,-1,&result);
			if ( bAddRef )
				sq_addref(m_hVM, &result);
			sq_pop(m_hVM,1);
		}
		sq_pop(m_hVM,1);
		return result;
	}

	HSCRIPT LookupFunction( const char *pszFunction, HSCRIPT hScope = nullptr ) override
	{
		HSQOBJECT result = LookupObject( pszFunction, hScope );
		if ( !sq_isnull( result ) )
		{
			if ( sq_isclosure(result) )
			{
				auto *pResult = new HSQOBJECT;
				*pResult = result;
				return (HSCRIPT)pResult;
			}
			sq_release( m_hVM, &result );
		}
		return nullptr;
	}

	void ReleaseFunction( HSCRIPT hScript ) override
	{
		ReleaseScriptObject( hScript );
	}

	ScriptStatus_t ExecuteFunction( HSCRIPT hFunction, ScriptVariant_t *pArgs, intp nArgs, ScriptVariant_t *pReturn, HSCRIPT hScope = nullptr, bool bWait = true ) override
	{
		if ( hScope == INVALID_HSCRIPT )
		{
			DWarning( "vscript:squirrel", 0, "Invalid scope handed to script VM.\n" );
			return SCRIPT_ERROR;
		}
		if ( m_hDbg )
		{
			// TODO: How to implement this without squirrel src modification?
			/*extern bool g_bSqDbgTerminateScript;
			if ( g_bSqDbgTerminateScript )
			{
				DisconnectDebugger();
				g_bSqDbgTerminateScript = false;
			}*/
		}

		Assert( bWait );
		if ( hFunction )
		{
			SQInteger initialTop = m_hVM->_top;
			HSQOBJECT &o = *((HSQOBJECT *)hFunction);
			Assert( bWait );

			sq_pushobject( m_hVM, o);
			if ( hScope ) 
			{
				if ( hScope == INVALID_HSCRIPT || *((HSQOBJECT *)hScope) == INVALID_HSQOBJECT || !sq_istable( *((HSQOBJECT *)hScope) ) )
				{
					sq_pop(m_hVM,1);
					return SCRIPT_ERROR;
				}
				sq_pushobject( m_hVM, *((HSQOBJECT *)hScope) );
			}
			else
			{
				sq_pushroottable( m_hVM );
			}

			for ( intp i = 0; i < nArgs; i++ )
			{
				PushVariant( pArgs[i], true );
			}

			m_TimeStartExecute = Plat_FloatTime();
			if (SQ_SUCCEEDED(sq_call(m_hVM, 1+nArgs, pReturn != NULL, SQ_CALL_RAISE_ERROR))) 
			{
				m_TimeStartExecute = 0.0;
				if ( pReturn )
				{
					HSQOBJECT ret;
					sq_getstackobj(m_hVM,-1,&ret);
					if ( !ConvertToVariant( ret, pReturn ) )
					{
						DWarning( "vscript:squirrel", 0, "Script function returned unsupported type\n" );
					}

					sq_pop(m_hVM,2);
				}
				else
				{
					sq_pop(m_hVM,1);
				}
				if ( m_hVM->_top != initialTop )
				{
					DWarning( "vscript:squirrel", 0, "Callstack mismatch in VScript/Squirrel!\n" );
					Assert( m_hVM->_top == initialTop );
				}
				if ( !sq_isnull( m_ErrorString ) )
				{
					if ( sq_isstring( m_ErrorString ) )
					{
						sq_throwerror( m_hVM, m_ErrorString._unVal.pString->_val );
					}
					else
					{
						sq_throwerror( m_hVM, "Internal error" );
					}
					m_ErrorString = NULL_HSQOBJECT;
					return SCRIPT_ERROR;
				}
				return SCRIPT_DONE;
			}
			m_TimeStartExecute = 0.0;
			sq_pop(m_hVM,1);
		}

		if ( pReturn )
		{
			pReturn->m_type = FIELD_VOID;
		}
		return SCRIPT_ERROR;
	}

	void RegisterFunction( ScriptFunctionBinding_t *pScriptFunction ) override
	{
		sq_pushroottable( m_hVM );
		RegisterFunctionGuts( pScriptFunction );
		sq_pop( m_hVM, 1 );
	}

	bool RegisterClass( ScriptClassDesc_t *pClassDesc ) override
	{
		if ( m_TypeMap.Find( (intptr_t)pClassDesc ) != m_TypeMap.InvalidHandle() )
		{
			return true;
		}

		sq_pushroottable( m_hVM );
		sq_pushstring( m_hVM, pClassDesc->m_pszScriptName, -1 );
		if ( sq_get( m_hVM, -2 ) == SQ_OK )
		{
			sq_pop( m_hVM, 2 );
			return false;
		}
		sq_pop( m_hVM, 1 );

		if ( pClassDesc->m_pBaseDesc )
		{
			CSquirrelVM::RegisterClass( pClassDesc->m_pBaseDesc );
		}

		int top = sq_gettop(m_hVM);

		HSQOBJECT newClass;
		newClass = CreateClass( pClassDesc );
		if ( newClass != INVALID_HSQOBJECT )
		{
			sq_pushobject( m_hVM, newClass );
			if ( pClassDesc->m_pfnConstruct )
			{
				sq_pushstring( m_hVM, "constructor", -1 );
				void **pUserData = (void **)sq_newuserdata(m_hVM, sizeof(void *));
				*pUserData = pClassDesc;
				sq_newclosure( m_hVM, &CallConstructor, 1 );
				sq_createslot( m_hVM, -3 );
			}

 			sq_pushstring( m_hVM, "_tostring", -1 );
 			sq_newclosure( m_hVM, &InstanceToString, 0 );
 			sq_createslot( m_hVM, -3 );

			sq_pushstring( m_hVM, "IsValid", -1 );
			sq_newclosure( m_hVM, &InstanceIsValid, 0 );
			sq_createslot( m_hVM, -3 );

			for ( int i = 0; i < pClassDesc->m_FunctionBindings.Count(); i++ )
			{
				RegisterFunctionGuts( &pClassDesc->m_FunctionBindings[i], pClassDesc );
			}
			sq_pop( m_hVM, 1 );
			// more setup required for inheritance?
		}
		sq_settop(m_hVM, top);
		m_TypeMap.Insert( (intptr_t)pClassDesc, newClass._unVal.pClass );
		return true;
	}

	bool CreateNativeInstance( ScriptClassDesc_t *pDesc, SQUserPointer ud,SQRELEASEHOOK hook )
	{
		sq_pushobject( m_hVM, SQObjectPtr(m_TypeMap[m_TypeMap.Find((intptr_t)pDesc)]) );
		if(SQ_FAILED(sq_createinstance(m_hVM,-1))) 
		{
			sq_pop( m_hVM, 1 );
			return false;
		}
		sq_remove(m_hVM,-2); //removes the class
		if(SQ_FAILED(sq_setinstanceup(m_hVM,-1,ud))) 
		{
			return false;
		}
		sq_setreleasehook(m_hVM,-1,hook);
		return TRUE;
	}

	HSCRIPT RegisterInstance( ScriptClassDesc_t *pDesc, void *pInstance ) override
	{
		if ( !CSquirrelVM::RegisterClass( pDesc ) )
		{
			return nullptr;
		}

		auto *pInstanceContext = new InstanceContext_t;
		pInstanceContext->pInstance = pInstance;
		pInstanceContext->pClassDesc = pDesc;
		pInstanceContext->name = NULL_HSQOBJECT;

		if ( !CreateNativeInstance( pDesc, pInstanceContext, &ExternalInstanceReleaseHook ) )
		{
			delete pInstanceContext;
			return nullptr;
		}

		HSQOBJECT hObject;
		sq_getstackobj(m_hVM,-1,&hObject);
		sq_addref( m_hVM, &hObject );
		sq_pop( m_hVM, 1 );

		auto *pResult = new HSQOBJECT;
		*pResult = hObject;
		return (HSCRIPT)pResult;
	}

	void SetInstanceUniqeId( HSCRIPT hInstance, const char *pszId ) override
	{
		if ( !hInstance )
		{
			ExecuteOnce( DMsg( "vscript:squirrel", 0, "NULL instance passed to vscript!\n" ) );
			return;
		}
		HSQOBJECT *pInstance = (HSQOBJECT *)hInstance;
		Assert( pInstance->_type == OT_INSTANCE );
		if ( pInstance->_type == OT_INSTANCE )
			((InstanceContext_t *)(pInstance->_unVal.pInstance->_userpointer))->name = SQString::Create( _ss(m_hVM), pszId );
	}

	void RemoveInstance( HSCRIPT hInstance ) override
	{
		if ( !hInstance )
		{
			ExecuteOnce( DMsg( "vscript:squirrel", 0, "NULL instance passed to vscript!\n" ) );
			return;
		}
		HSQOBJECT *pInstance = (HSQOBJECT *)hInstance;
		Assert( pInstance->_type == OT_INSTANCE );
		if ( pInstance->_type == OT_INSTANCE )
			((InstanceContext_t *)(pInstance->_unVal.pInstance->_userpointer))->pInstance = nullptr;
		ReleaseScriptObject( hInstance );
	}

	void *GetInstanceValue( HSCRIPT hInstance, ScriptClassDesc_t *pExpectedType ) override
	{
		if ( !hInstance )
		{
			ExecuteOnce( DMsg( "vscript:squirrel", 0, "NULL instance passed to vscript!\n" ) );
			return nullptr;
		}
		HSQOBJECT *pInstance = (HSQOBJECT *)hInstance;
		if ( pInstance->_type == OT_INSTANCE && pInstance->_unVal.pInstance->_userpointer )
		{
			InstanceContext_t *pContext = ((InstanceContext_t *)(pInstance->_unVal.pInstance->_userpointer));
			if ( !pExpectedType || pContext->pClassDesc == pExpectedType || IsClassDerivedFrom( pContext->pClassDesc, pExpectedType ) )
				return pContext->pInstance;
		}
		return nullptr;
	}

	bool IsClassDerivedFrom( const ScriptClassDesc_t *pDerivedClass, const ScriptClassDesc_t *pBaseClass )
	{
		const ScriptClassDesc_t* pType = pDerivedClass->m_pBaseDesc;
		while ( pType )
		{
			if ( pType == pBaseClass )
				return true;

			pType = pType->m_pBaseDesc;
		}
		
		return false;
	}

	bool GenerateUniqueKey( const char *pszRoot, char *pBuf, int nBufSize ) override
	{
		const intp minSize = V_strlen(pszRoot) + 40 + 1;
		Assert( minSize <= nBufSize );
		if ( minSize <= nBufSize )
		{
			Q_snprintf( pBuf, nBufSize, "%x%x%llx_%s", RandomInt(0, 0xfff), Plat_MSTime(), m_iUniqueIdSerialNumber++, pszRoot ); // random to limit key compare when serial number gets large
			return true;
		}
		if ( nBufSize )
		{
			*pBuf = 0;
		}
		Warning( "squirrel: GenerateUniqueKey: buffer is too small. Need at least %zd, got %d.\n", minSize, nBufSize );
		return false;
	}

	bool ValueExists(  HSCRIPT hScope, const char *pszKey ) override
	{
		return !sq_isnull( LookupObject( pszKey, hScope, false ) );
	}


	bool SetValue( HSCRIPT hScope, const char *pszKey, const char *pszValue ) override
	{
		if ( !hScope )
		{
			sq_pushroottable( m_hVM );
		}
		else
		{
			if ( hScope == INVALID_HSCRIPT || *((HSQOBJECT *)hScope) == INVALID_HSQOBJECT || !sq_istable( *((HSQOBJECT *)hScope) ) )
			{
				return false;
			}
			sq_pushobject( m_hVM, *((HSQOBJECT *)hScope) );
		}

		sq_pushstring( m_hVM, pszKey, -1 );
		sq_pushstring( m_hVM, pszValue, -1 );

		sq_createslot( m_hVM, -3 );
		sq_pop( m_hVM, 1 );

		return true;
	}

	bool SetValue( HSCRIPT hScope, const char *pszKey, const ScriptVariant_t &value ) override
	{
		if ( !hScope )
		{
			sq_pushroottable( m_hVM );
		}
		else
		{
			if ( hScope == INVALID_HSCRIPT || *((HSQOBJECT *)hScope) == INVALID_HSQOBJECT || !sq_istable( *((HSQOBJECT *)hScope) ) )
			{
				return false;
			}
			sq_pushobject( m_hVM, *((HSQOBJECT *)hScope) );
		}

		sq_pushstring( m_hVM, pszKey, -1 );
		if ( value.m_type == FIELD_HSCRIPT && value.m_hScript )
		{
			HSQOBJECT hObject = *((HSQOBJECT *)value.m_hScript);
			if ( sq_isinstance( hObject ) )
			{
				SQInstance *pInstance = hObject._unVal.pInstance;
				if ( pInstance->_class->_typetag && pInstance->_class->_typetag != TYPETAG_VECTOR )
				{
					auto *pContext = (InstanceContext_t *)pInstance->_userpointer;
					if ( sq_isnull( pContext->name ) )
					{
						pContext->name = m_hVM->_stack[m_hVM->_top - 1];
					}
				}
			}
		}
		PushVariant( value, true );
		sq_createslot( m_hVM, -3 );
		sq_pop(m_hVM,1);

		return true;
	}


	void CreateTable( ScriptVariant_t &Table ) override
	{
		HSQOBJECT hObject;

		sq_newtable( m_hVM );
		sq_getstackobj(m_hVM, -1, &hObject );
		sq_addref( m_hVM, &hObject );

		ConvertToVariant( hObject, &Table );
		sq_pop( m_hVM, 1 ); 
	}


	//------------------------------------------------------------------------------
	// Purpose: returns the number of elements in the table
	// Input  : hScope - the table
	// Output : returns the number of elements in the table
	//------------------------------------------------------------------------------
	int	GetNumTableEntries( HSCRIPT hScope ) override
	{
		if ( !hScope )
		{
			sq_pushroottable( m_hVM );
		}
		else
		{
			if ( hScope == INVALID_HSCRIPT || *((HSQOBJECT *)hScope) == INVALID_HSQOBJECT || !sq_istable( *((HSQOBJECT *)hScope) ) )
			{
				return 0;
			}
			sq_pushobject( m_hVM, *((HSQOBJECT *)hScope) );
		}

		int nCount = sq_getsize( m_hVM, -1 );

		sq_pop( m_hVM , 1 );

		return nCount;
	}


	// Purpose: Gets a key / value pair from the table
	// Input  : hScope - the table
	//			nInterator - the current location inside of the table.  NOTE this is nota linear representation
	// Output : returns the next iterator spot, otherwise -1 if error or end of table
	//			pKey - the key entry
	//			pValue - the value entry
	int GetKeyValue( HSCRIPT hScope, int nIterator, ScriptVariant_t *pKey, ScriptVariant_t *pValue ) override
	{
		HSQOBJECT KeyResult = { OT_NULL, NULL };
		HSQOBJECT ValueResult = { OT_NULL, NULL };

		if ( !hScope )
		{
			sq_pushroottable( m_hVM );
		}
		else
		{
			if ( hScope == INVALID_HSCRIPT || *((HSQOBJECT *)hScope) == INVALID_HSQOBJECT || !sq_istable( *((HSQOBJECT *)hScope) ) )
			{
				return -1;
			}
			sq_pushobject( m_hVM, *((HSQOBJECT *)hScope) );
		}

		intp nReturnValue;

		sq_pushinteger(m_hVM, nIterator);

		if ( SQ_SUCCEEDED(sq_next(m_hVM,-2) ) )
		{
			sq_getstackobj(m_hVM,-2, &KeyResult );
			sq_getstackobj(m_hVM,-1, &ValueResult );
			sq_addref( m_hVM,&KeyResult );
			sq_addref( m_hVM,&ValueResult );

			ConvertToVariant( KeyResult, pKey );
			ConvertToVariant( ValueResult, pValue );

			sq_pop(m_hVM,2); //pops key and val before the nex iteration

			sq_getinteger(m_hVM, -1, &nReturnValue);
		}
		else
		{
			nReturnValue = -1;
		}
		sq_pop( m_hVM, 1 ); //pops the null iterator
		sq_pop( m_hVM, 1 );
			
		return nReturnValue;
	}


	bool GetValue( HSCRIPT hScope, const char *pszKey, ScriptVariant_t *pValue ) override
	{
		HSQOBJECT result = LookupObject( pszKey, hScope );
		if ( ConvertToVariant( result, pValue ) && !sq_isnull( result ) )
		{
			return true;
		}
		__Release( result._type, result._unVal );
		return false;
	}

	bool ClearValue( HSCRIPT hScope, const char *pszKey ) override
	{
		if ( !hScope )
		{
			sq_pushroottable( m_hVM );
		}
		else
		{
			if ( hScope == INVALID_HSCRIPT || *((HSQOBJECT *)hScope) == INVALID_HSQOBJECT || !sq_istable( *((HSQOBJECT *)hScope) ) )
			{
				return false;
			}
			sq_pushobject( m_hVM, *((HSQOBJECT *)hScope) );
		}
		sq_pushstring( m_hVM, pszKey, -1 );
		sq_deleteslot( m_hVM, -2, false );
		sq_pop(m_hVM,1);
		return false;
	}

	void ReleaseValue( ScriptVariant_t &value ) override
	{
		if ( value.m_type == FIELD_HSCRIPT )
		{
			sq_release( m_hVM, (HSQOBJECT *)value.m_hScript );
			delete ((HSQOBJECT *)value.m_hScript);
		}
		else
		{
			value.Free();
		}
		value.m_type = FIELD_VOID;
	}

	bool RaiseException( const char *pszExceptionText ) override
	{
		m_ErrorString = SQString::Create( m_hVM->_sharedstate, pszExceptionText );
		return true;
	}

	void DumpState() override
	{
		// TODO: How to do it?
		/*struct CIterator final : public CSQStateIterator
		{
			explicit CIterator( HSQUIRRELVM hVM )
			{
				indent = 0;
				m_hVM = hVM;
				m_bKey = false;
			}

			void Indent()
			{
				for ( int i = 0; i < indent; i++)
				{
					DMsg( "vscript:squirrel", 0, "  " );
				}
			}

			void PsuedoKey( const char *pszPsuedoKey ) override
			{
				Indent();
				DMsg( "vscript:squirrel", 0, "%s: ", pszPsuedoKey );
				m_bKey = true;
			}

			void Key( SQObjectPtr &key ) override
			{
				Indent();
				SQObjectPtr res;
				m_hVM->ToString( key, res );
				DMsg( "vscript:squirrel", 0, "%s: ", res._unVal.pString->_val );
				m_bKey = true;
			}

			void Value( SQObjectPtr &value ) override
			{
				if ( !m_bKey )
				{
					Indent();
				}
				m_bKey = false;
				SQObjectPtr res;
				m_hVM->ToString( value, res );
				if ( ISREFCOUNTED(value._type) )
					DMsg( "vscript:squirrel", 0, "%s [%d]\n", res._unVal.pString->_val, value._unVal.pRefCounted->_uiRef );
				else
					DMsg( "vscript:squirrel", 0, "%s\n", res._unVal.pString->_val );
			}

			bool BeginContained() override
			{
				if ( m_bKey )
				{
					DMsg( "vscript:squirrel", 0, "\n" );
				}
				m_bKey = false;
				Indent();
				DMsg( "vscript:squirrel", 0, "{\n" );
				indent++;
				return true;
			}

			void EndContained() override
			{
				indent--;
				Indent();
				DMsg( "vscript:squirrel", 0, "}\n" );
			}

			int indent;
			HSQUIRRELVM m_hVM;
			bool m_bKey;
		};

		CIterator iter( m_hVM );
		m_hVM->_sharedstate->Iterate( m_hVM, &iter );*/
	}

	void WriteState( CUtlBuffer *pBuffer ) override
	{
#ifdef VSQUIRREL_DEBUG_SERIALIZATION
		DMsg( "vscript:squirrel", 0, "BEGIN WRITE\n" );
#endif
		m_pBuffer = pBuffer;
		sq_collectgarbage( m_hVM );

		m_pBuffer->PutInt( SAVEVERSION );
		m_pBuffer->PutInt64( (int64)m_iUniqueIdSerialNumber );
		WriteVM( m_hVM );

		m_pBuffer = nullptr;

		SQCollectable *t = m_hVM->_sharedstate->_gc_chain;
		while(t) 
		{
			t->UnMark();
			t = t->_next;
		}

		m_PtrMap.Purge();
	}

	void ReadState( CUtlBuffer *pBuffer ) override
	{
#ifdef VSQUIRREL_DEBUG_SERIALIZATION
#ifdef VSQUIRREL_DEBUG_SERIALIZATION_HEAPCHK
		g_pMemAlloc->CrtCheckMemory();
		int flags = g_pMemAlloc->CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );
		g_pMemAlloc->CrtSetDbgFlag( flags | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_CHECK_CRT_DF );
#endif
		DMsg( "vscript:squirrel", 0, "BEGIN READ\n" );
#endif

		if ( const int version = pBuffer->GetInt(); version != SAVEVERSION )
		{
			DMsg( "vscript:squirrel",
				0,
				"Incompatible script version. Expected 0x%x, got 0x%x.\n",
				SAVEVERSION,
				version );
			return;
		}
		sq_collectgarbage( m_hVM );
		//m_hVM->_sharedstate->_gc_disableDepth++;
		m_pBuffer = pBuffer;
		auto uniqueIdSerialNumber = (uint64)m_pBuffer->GetInt64();
		m_iUniqueIdSerialNumber = max( m_iUniqueIdSerialNumber, uniqueIdSerialNumber );
		Verify( pBuffer->GetInt() == OT_THREAD );
		m_PtrMap.Insert( pBuffer->GetPtr(), m_hVM );
		ReadVM( m_hVM );
		m_pBuffer = nullptr;
		m_PtrMap.Purge();
		// m_hVM->_sharedstate->_gc_disableDepth--;
		sq_collectgarbage( m_hVM );

#ifdef VSQUIRREL_DEBUG_SERIALIZATION_HEAPCHK
		g_pMemAlloc->CrtSetDbgFlag( flags );
#endif
	}

	void RemoveOrphanInstances() override
	{
	}

	void SetOutputCallback( ScriptOutputFunc_t pFunc ) override
	{
	}

	void SetErrorCallback( ScriptErrorFunc_t pFunc ) override
	{
	}

private:
	struct InstanceContext_t
	{
		void *pInstance;
		ScriptClassDesc_t *pClassDesc;
		SQObjectPtr name;
	};

	//---------------------------------------------------------
	// Callbacks
	//---------------------------------------------------------
	static void PrintHandler(HSQUIRRELVM m_hVM,const SQChar* s,...)
	{
		char string[2048];
		va_list		argptr;
		va_start (argptr,s); //-V2019 //-V2018
		V_vsprintf_safe (string,s,argptr);
		va_end (argptr);
		
		DMsg( "vscript:squirrel", 0, "%s.\n", string );
	}

	static void ErrorHandler(HSQUIRRELVM m_hVM,const SQChar* s,...)
	{
		char string[2048];
		va_list		argptr;
		va_start (argptr,s); //-V2019 //-V2018
		V_vsprintf_safe (string,s,argptr);
		va_end (argptr);
		
		DWarning( "vscript:squirrel", 0, "%s.\n", string );
	}

	static void CompileErrorHandler(HSQUIRRELVM m_hVM,
		const SQChar *desc,
		const SQChar *source,
		SQInteger line,
		SQInteger column)
	{
		DWarning( "vscript:squirrel",
			0,
			"%s(%d,%d): compile error: %s.\n",
			source, desc, line, column );
	}

	static SQInteger RuntimeErrorHandler(HSQUIRRELVM m_hVM)
	{
		if ( sq_gettop( m_hVM ) <= 0 )
		{
			DWarning( "vscript:squirrel", 0, "runtime error: unknown.\n" );
			return 0;
		}

		CUtlString msg;

		if ( const SQChar *err{nullptr}; SQ_SUCCEEDED( sq_getstring( m_hVM, -1, &err ) ) )
		{
			msg += err;
			msg += "\n";
		}
		
		char line[12];
		SQStackInfos si;
		// Unwind the stack.
		SQInteger stackLevel = sq_gettop( m_hVM ), lineNo = 1;
		while ( stackLevel >= 1 )
		{
			V_to_chars( line, lineNo );
			msg += line;
			msg += "#   ";

			if ( SQ_SUCCEEDED( sq_stackinfos( m_hVM, stackLevel, &si ) ) )
			{
				if ( si.funcname )
				{
					msg += si.funcname;
					msg += "() ";
				}
				else
				{
					msg += "unknown";
				}

				if ( si.source )
				{
					msg += "at ";
					msg += si.source;
				}
				else
				{
					msg += "unknown";
				}
				
				msg += ":";

				V_to_chars( line, si.line );
				msg.Append( line );
			}
			else
			{
				msg += "<missed stack trace frames>";
			}

			--stackLevel;
			++lineNo;
		} 

		DWarning( "vscript:squirrel", 0, "runtime error: %s.\n", !msg.IsEmpty() ? msg.Get() : "unknown" );

		return 0;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	static SQInteger ReleaseHook( SQUserPointer p, SQInteger size )
	{
		auto *pInstanceContext = (InstanceContext_t *)p;
		pInstanceContext->pClassDesc->m_pfnDestruct( pInstanceContext->pInstance );
		delete pInstanceContext;
		return 0;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	static SQInteger ExternalInstanceReleaseHook( SQUserPointer p, SQInteger size )
	{
		auto *pInstanceContext = (InstanceContext_t *)p;
		delete pInstanceContext;
		return 0;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	static SQInteger GetFunctionSignature( HSQUIRRELVM hVM )
	{
		StackHandler sa(hVM);
		if ( sa.GetParamCount() != 3 )
		{
			return 0;
		}

		HSQOBJECT hFunction = sa.GetObjectHandle( 2 );
		if ( !sq_isclosure( hFunction ) )
		{
			return 0;
		}

		char result[ 512 ] = {0};
		const char *pszName = sa.GetString( 3 );
		SQClosure *pClosure = hFunction._unVal.pClosure;
		SQFunctionProto *pProto = pClosure->_function;

		V_strcat_safe( result, "function " );
		if ( pszName && *pszName )
		{
			V_strcat_safe( result, pszName );
		}
		else if ( sq_isstring( pProto->_name ) )
		{
			V_strcat_safe( result,
				pProto->_name._unVal.pString->_val );
		}
		else
		{
			V_strcat_safe( result, "<unnamed>" );
		}
		V_strcat_safe( result, "(" );

		for ( int i = 1; i < pProto->_nparameters; i++ )
		{
			if ( i != 1 )
			{
				V_strcat_safe( result, ", " );
			}
			if ( sq_isstring( pProto->_parameters[i] ) )
			{
				V_strcat_safe( result,
					pProto->_parameters[i]._unVal.pString->_val );
			}
			else
			{
				V_strcat_safe( result, "arg" );
			}
		}
		V_strcat_safe( result, ")" );

		result[ sizeof( result ) - 1 ] = 0;
		sa.Return( result );

		return 1;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	static SQInteger GetDeveloper( HSQUIRRELVM hVM )
	{
		StackHandler sa(hVM);
		sa.Return( ((CSquirrelVM *)_ss( hVM )->_foreignptr)->developer.GetInt() );
		return 1;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	static SQInteger CallConstructor( HSQUIRRELVM hVM )
	{
		StackHandler sa(hVM);
		int nActualParams = sa.GetParamCount();
		ScriptClassDesc_t *pClassDesc = *((ScriptClassDesc_t **)sa.GetUserData( nActualParams ));
		auto *pInstanceContext = new InstanceContext_t;
		pInstanceContext->pInstance = pClassDesc->m_pfnConstruct();
		pInstanceContext->pClassDesc = pClassDesc;
		sq_setinstanceup(hVM, 1, pInstanceContext);
		sq_setreleasehook( hVM, 1, &ReleaseHook );
		return 0;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	static SQInteger TranslateCall( HSQUIRRELVM hVM )
	{
		StackHandler sa(hVM);
		int nActualParams = sa.GetParamCount();
		ScriptFunctionBinding_t *pVMScriptFunction = *((ScriptFunctionBinding_t **)sa.GetUserData( nActualParams ));
		int nFormalParams = pVMScriptFunction->m_desc.m_Parameters.Count();
		CUtlVectorFixed<ScriptVariant_t, 14> params;
		ScriptVariant_t returnValue;
		bool bCallFree = false;

		params.SetSize( nFormalParams );

		int i = 0;

		if ( nActualParams )
		{
			int iLimit = MIN( nActualParams, nFormalParams );
			ScriptDataType_t *pCurParamType = pVMScriptFunction->m_desc.m_Parameters.Base();
			for ( i = 0; i < iLimit; i++, pCurParamType++ )
			{
				switch ( *pCurParamType )
				{
				case FIELD_FLOAT:		params[i] = sa.GetFloat( i + 2 ); break;
				case FIELD_CSTRING:		params[i] = sa.GetString( i + 2 ); break;
				case FIELD_VECTOR:		
					{
						Vector vec{};
						if (const auto rc = vsq_getvec3( hVM, sa, i + 2, vec ); SQ_FAILED(rc)) {
							return rc;
						}

						params[i] = vec;
					}
				case FIELD_INTEGER:		params[i] = sa.GetInt( i + 2 ); break;
				case FIELD_BOOLEAN:		params[i] = sa.GetBool( i + 2 ); break;
				case FIELD_CHARACTER:	params[i] = sa.GetString( i + 2 )[0]; break;
				case FIELD_HSCRIPT:
					{
						HSQOBJECT object = sa.GetObjectHandle( i+2 );
						if ( object._type == OT_NULL)
						{
							params[i] = (HSCRIPT)nullptr;
						}
						else
						{
							auto *pObject = new HSQOBJECT;
							*pObject = object;
							params[i] = (HSCRIPT)pObject;
							params[i].m_flags |= SV_FREE;
							bCallFree = true;
						}
						break;
					}
				default:				break;
				}
			}
		}

#ifdef _DEBUG
		for ( ; i < nFormalParams; i++ )
		{
			Assert( params[i].IsNull() );
		}
#endif

		InstanceContext_t *pContext;
		void *pObject;

		if ( pVMScriptFunction->m_flags & SF_MEMBER_FUNC )
		{
			pContext = (InstanceContext_t *)sa.GetInstanceUp(1,0);

			if ( !pContext )
			{
				sq_throwerror( hVM, "Accessed null instance" );
				return SQ_ERROR;
			}

			pObject = pContext->pInstance;

			if ( !pObject )
			{
				sq_throwerror( hVM, "Accessed null instance" );
				return SQ_ERROR;
			}

			if ( pContext->pClassDesc->pHelper )
			{
				pObject = pContext->pClassDesc->pHelper->GetProxied( pObject, pVMScriptFunction );
			}

			if ( !pObject )
			{
				sq_throwerror( hVM, "Accessed null instance" );
				return SQ_ERROR;
			}
		}
		else
		{
			pObject = nullptr;
		}

		(*pVMScriptFunction->m_pfnBinding)( pVMScriptFunction->m_pFunction, pObject, params.Base(), params.Count(), ( pVMScriptFunction->m_desc.m_ReturnType != FIELD_VOID ) ? &returnValue : NULL );

		if ( pVMScriptFunction->m_desc.m_ReturnType != FIELD_VOID )
		{
			switch ( returnValue.m_type )
			{
			case FIELD_FLOAT:		sa.Return( (float)returnValue ); break;
			case FIELD_CSTRING:		sa.Return( (const char *)returnValue ); break;
			case FIELD_VECTOR:		
				{
					sq_pushobject( hVM, ((CSquirrelVM *)_ss( hVM )->_foreignptr)->m_hClassVector );
					sq_createinstance( hVM, -1 );
					sq_setinstanceup( hVM, -1, (SQUserPointer)returnValue.m_pVector );
					sq_setreleasehook( hVM, -1, &vsq_releasevec3 );
					sq_remove( hVM, -2 );
					break;
				}
			case FIELD_INTEGER:		sa.Return( (int)returnValue ); break;
			case FIELD_BOOLEAN:		sa.Return( (bool)returnValue ); break;
			case FIELD_CHARACTER:	Assert( 0 ); sq_pushnull( hVM ); break;
			case FIELD_HSCRIPT:
				{
					if ( returnValue.m_hScript )
					{
						sq_pushobject( hVM, *((HSQOBJECT *)returnValue.m_hScript) );
					}
					else
					{
						sq_pushnull( hVM );
					}
					break;
				}
			default:				sq_pushnull( hVM ); break;
			}
		}

		if ( bCallFree )
		{
			for ( i = 0; i < params.Count(); i++ )
			{
				params[i].Free();
			}
		}

		if ( !sq_isnull( ((CSquirrelVM *)_ss( hVM )->_foreignptr)->m_ErrorString ) )
		{
			if ( sq_isstring( ((CSquirrelVM *)_ss( hVM )->_foreignptr)->m_ErrorString ) )
			{
				sq_throwerror( hVM, ((CSquirrelVM *)_ss( hVM )->_foreignptr)->m_ErrorString._unVal.pString->_val );
			}
			else
			{
				sq_throwerror( hVM, "Internal error" );
			}
			((CSquirrelVM *)_ss( hVM )->_foreignptr)->m_ErrorString = NULL_HSQOBJECT;
			return SQ_ERROR;
		}

		return ( pVMScriptFunction->m_desc.m_ReturnType != FIELD_VOID );
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	static SQInteger InstanceToString( HSQUIRRELVM hVM )
	{
		StackHandler sa(hVM);
		auto *pContext = (InstanceContext_t *)sa.GetInstanceUp(1,0);
		char szBuf[64];

		if ( pContext && pContext->pInstance && pContext->pClassDesc->pHelper && pContext->pClassDesc->pHelper->ToString( pContext->pInstance, szBuf, ARRAYSIZE(szBuf) ) )
		{
			sa.Return( szBuf );
		}
		else
		{
			HSQOBJECT hInstance = sa.GetObjectHandle( 1 );
			sq_pushstring( hVM, CFmtStr( "(instance : 0x%p)", (void*)_rawval(hInstance) ), -1 );
		}
		return 1;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	static SQInteger InstanceIsValid( HSQUIRRELVM hVM )
	{
		StackHandler sa(hVM);
		auto *pContext = (InstanceContext_t *)sa.GetInstanceUp(1,0);
		sq_pushbool( hVM, ( pContext && pContext->pInstance ) );
		return 1;
	}



	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	HSQOBJECT CreateClass( ScriptClassDesc_t *pDesc )
	{
		int oldtop = sq_gettop(m_hVM);
		sq_pushroottable(m_hVM);
		sq_pushstring(m_hVM,pDesc->m_pszScriptName,-1);
		if (pDesc->m_pBaseDesc)
		{
			sq_pushstring(m_hVM,pDesc->m_pBaseDesc->m_pszScriptName,-1);
			if (SQ_FAILED(sq_get(m_hVM,-3))) 
			{ // Make sure the base exists if specified by baseName.
				sq_settop(m_hVM,oldtop);
				return INVALID_HSQOBJECT;
			}
		}

		if (SQ_FAILED(sq_newclass(m_hVM,pDesc->m_pBaseDesc ? 1 : 0))) 
		{ // Will inherit from base class on stack from sq_get() above.
			sq_settop(m_hVM,oldtop);
			return INVALID_HSQOBJECT;
		}
		HSQOBJECT hObject;
		sq_getstackobj(m_hVM,-1, &hObject);
		sq_addref(m_hVM, &hObject);
		sq_settypetag(m_hVM,-1,pDesc);
		sq_createslot(m_hVM,-3);
		sq_pop(m_hVM,1);
		return hObject;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void RegisterFunctionGuts( ScriptFunctionBinding_t *pScriptFunction, ScriptClassDesc_t *pClassDesc = nullptr )
	{
		char szTypeMask[64];

		if ( pScriptFunction->m_desc.m_Parameters.Count() > ARRAYSIZE(szTypeMask) - 1 )
		{
			AssertMsg1( 0, "Too many agruments for script function %s\n", pScriptFunction->m_desc.m_pszFunction );
			return;
		}

		szTypeMask[0] = '.';
		char *pCurrent = &szTypeMask[1];
		for ( int i = 0; i < pScriptFunction->m_desc.m_Parameters.Count(); i++, pCurrent++ )
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
		Assert( pCurrent - szTypeMask < ARRAYSIZE(szTypeMask) - 1 );
		*pCurrent = 0;
		sq_pushstring( m_hVM, pScriptFunction->m_desc.m_pszScriptName, -1 );
		ScriptFunctionBinding_t **pVMScriptFunction = (ScriptFunctionBinding_t **)sq_newuserdata(m_hVM, sizeof(ScriptFunctionBinding_t *));
		*pVMScriptFunction = pScriptFunction;
		sq_newclosure( m_hVM, &TranslateCall, 1 );
		HSQOBJECT hFunction;
		sq_getstackobj( m_hVM, -1, &hFunction );
		sq_setnativeclosurename(m_hVM, -1, pScriptFunction->m_desc.m_pszScriptName );
		sq_setparamscheck( m_hVM, pScriptFunction->m_desc.m_Parameters.Count() + 1, szTypeMask );
		sq_createslot( m_hVM, -3 );

		if ( developer.GetInt() )
		{
			const char *pszHide = SCRIPT_HIDE;
			if ( !pScriptFunction->m_desc.m_pszDescription || *pScriptFunction->m_desc.m_pszDescription != *pszHide )
			{
				char name[512] = {0};
				char signature[512] = {0};

				if ( pClassDesc )
				{
					V_strcat_safe( name,
						pClassDesc->m_pszScriptName );
					V_strcat_safe( name,
						"::" );
				}

				V_strcat_safe( name,
					pScriptFunction->m_desc.m_pszScriptName );

				V_strcat_safe( signature,
					FieldTypeToString( pScriptFunction->m_desc.m_ReturnType ) );
				V_strcat_safe( signature,
					" " );
				V_strcat_safe( signature,
					name );
				V_strcat_safe( signature,
					"(" );
				for ( int i = 0; i < pScriptFunction->m_desc.m_Parameters.Count(); i++ )
				{
					if ( i != 0 )
					{
						V_strcat_safe( signature,
							", " );
					}

					V_strcat_safe( signature,
						FieldTypeToString( pScriptFunction->m_desc.m_Parameters[i] ) );
				}
				V_strcat_safe( signature,
					")" );

				sq_pushobject( m_hVM, LookupObject( "RegisterFunctionDocumentation", NULL, false ) );
				sq_pushroottable( m_hVM );
				sq_pushobject( m_hVM, hFunction );
				sq_pushstring( m_hVM, name, -1 );
				sq_pushstring( m_hVM, signature, -1 );
				sq_pushstring( m_hVM, pScriptFunction->m_desc.m_pszDescription, -1 );
				sq_call( m_hVM, 5, false, /*false*/ true );
				sq_pop( m_hVM, 1 );
			}
		}
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void ReleaseScriptObject( HSCRIPT hScript )
	{
		if ( hScript )
		{
			HSQOBJECT *pScript = (HSQOBJECT *)hScript;
			sq_release( m_hVM, pScript );
			delete pScript;
		}
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void PushVariant( const ScriptVariant_t &value, bool bCopy = false )
	{
		switch ( value.m_type )
		{
		case FIELD_VOID:		sq_pushnull( m_hVM ); break;
		case FIELD_FLOAT:		sq_pushfloat( m_hVM, value ); break;
		case FIELD_CSTRING:		sq_pushstring( m_hVM, value, strlen( value.m_pszString ) ); break;
		case FIELD_VECTOR:
			{
				// @TODO: should make a pool of these and reuse [4/22/2008 tom]
				sq_pushobject( m_hVM, m_hClassVector );
				sq_createinstance( m_hVM, -1 );
				if ( !bCopy )
				{
					sq_setinstanceup( m_hVM, -1, (SQUserPointer)value.m_pVector );
				}
				else
				{
					sq_setinstanceup( m_hVM, -1, new Vector( *value.m_pVector ) );
					sq_setreleasehook( m_hVM, -1, &vsq_releasevec3 );
				}
				sq_remove( m_hVM, -2 );
				break;
			}
		case FIELD_INTEGER:		sq_pushinteger( m_hVM, value.m_int ); break;
		case FIELD_BOOLEAN:		sq_pushbool( m_hVM, value.m_bool ); break;
		case FIELD_CHARACTER:	{ char sz[2]; sz[0] = value.m_char; sz[1] = 0; sq_pushstring( m_hVM, sz, 1 ); break; }
		case FIELD_HSCRIPT:		if ( value.m_hScript ) sq_pushobject( m_hVM, *((HSQOBJECT *)value.m_hScript) ); else sq_pushnull( m_hVM ); break;
		}
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	bool ConvertToVariant( HSQOBJECT object, ScriptVariant_t *pReturn )
	{
		switch ( object._type )
		{
		case OT_NULL:		pReturn->m_type = FIELD_VOID; break;
		case OT_INTEGER:	*pReturn = object._unVal.nInteger; break;
		case OT_FLOAT:		*pReturn = object._unVal.fFloat; break;
		case OT_BOOL:		*pReturn = (object._unVal.nInteger != 0); break;
		case OT_STRING:		
			{ 
				SQInteger size = object._unVal.pString->_len + 1; 
				pReturn->m_type = FIELD_CSTRING;
				char *value = new char[size];
				pReturn->m_pszString = value;
				memcpy( value, object._unVal.pString->_val, size ); 
				pReturn->m_flags |= SV_FREE;
			}
			break;
		case OT_INSTANCE:
			{
				sq_pushobject( m_hVM, object );
				SQUserPointer pVector;
				SQRESULT rc = sq_getinstanceup( m_hVM, -1, &pVector, TYPETAG_VECTOR, 1 );
				sq_poptop( m_hVM );
				if ( SQ_SUCCEEDED( rc ) )
				{
					pReturn->m_type = FIELD_VECTOR;
					pReturn->m_pVector = new Vector( *((Vector *)pVector) ); 
					pReturn->m_flags |= SV_FREE;
					break;
				}
			}
			[[fallthrough]];
			// fall through
		default:
			{
				pReturn->m_type = FIELD_HSCRIPT;
				auto *pObject = new HSQOBJECT;
				*pObject = object;
				pReturn->m_hScript = (HSCRIPT)pObject;
			}
		}
		return true;
	}

	//-------------------------------------------------------------------------
	// Serialization
	//-------------------------------------------------------------------------
	enum
	{
		SAVEVERSION = 2
	};

	void WriteObject( const SQObjectPtr &object )
	{
		switch ( object._type )
		{
			case OT_NULL:
				m_pBuffer->PutInt( OT_NULL ); 
				break;
			case OT_INTEGER:
				m_pBuffer->PutInt( OT_INTEGER ); 
				m_pBuffer->PutInt( object._unVal.nInteger );
				break;
			case OT_FLOAT:
				m_pBuffer->PutInt( OT_FLOAT ); 
				m_pBuffer->PutFloat( object._unVal.fFloat);
				break;
			case OT_BOOL:			
				m_pBuffer->PutInt( OT_BOOL ); 
				m_pBuffer->PutInt( object._unVal.nInteger );
				break;
			case OT_STRING:			
				m_pBuffer->PutInt( OT_STRING ); 
				m_pBuffer->PutInt( object._unVal.pString->_len );
				m_pBuffer->PutString( object._unVal.pString->_val );	
				break;
			case OT_TABLE:			WriteTable( object._unVal.pTable );					break;
			case OT_ARRAY:			WriteArray( object._unVal.pArray );					break;
			case OT_USERDATA:		WriteUserData( object._unVal.pUserData );			break;
			case OT_CLOSURE:		WriteClosure( object._unVal.pClosure );				break;
			case OT_NATIVECLOSURE:	WriteNativeClosure( object._unVal.pNativeClosure );	break;
			case OT_GENERATOR:		WriteGenerator( object._unVal.pGenerator );			break;
			case OT_USERPOINTER:	WriteUserPointer( object._unVal.pUserPointer );		break;
			case OT_THREAD:			WriteVM( object._unVal.pThread );					break;
			case OT_FUNCPROTO:		WriteFuncProto( object._unVal.pFunctionProto );		break;
			case OT_CLASS:			WriteClass( object._unVal.pClass );					break;
			case OT_INSTANCE:		WriteInstance( object._unVal.pInstance );			break;
			case OT_WEAKREF:		WriteWeakRef( object._unVal.pWeakRef );				break;
			default:				Assert( 0 ); break;
		}

#ifdef VSQUIRREL_DEBUG_SERIALIZATION
		SQObjectPtr res;
		m_hVM->ToString( object, res );
		DMsg( "vscript:squirrel", 0, "%d: %s\n", m_pBuffer->TellPut(),  res._unVal.pString->_val );
#endif
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void WriteVM( SQVM *pVM )
	{
		unsigned i;

		m_pBuffer->PutInt( OT_THREAD );
		m_pBuffer->PutPtr( pVM );

		if ( pVM->_uiRef & MARK_FLAG )
			return;
		pVM->_uiRef |= MARK_FLAG;

		WriteObject( pVM->_roottable );
		m_pBuffer->PutInt( pVM->_top );
		m_pBuffer->PutInt( pVM->_stackbase );
		m_pBuffer->PutUnsignedInt( pVM->_stack.size() );
		for( i = 0; i < pVM->_stack.size(); i++ ) 
		{
			WriteObject( pVM->_stack[i] );
		}
		//m_pBuffer->PutUnsignedInt( pVM->_vargsstack.size() );
		//for( i = 0; i < pVM->_vargsstack.size(); i++ ) 
		//{
		//	WriteObject( pVM->_vargsstack[i] );
		//}
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void WriteArray( SQArray *pArray )
	{
		m_pBuffer->PutInt( OT_ARRAY );
		m_pBuffer->PutPtr( pArray );

		if ( pArray->_uiRef & MARK_FLAG )
			return;
		pArray->_uiRef |= MARK_FLAG;

		int len = pArray->_values.size();
		m_pBuffer->PutInt( len );
		for ( int i = 0; i < len; i++ )
			WriteObject( pArray->_values[i] );
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void WriteTable( SQTable *pTable )
	{
		m_pBuffer->PutInt( OT_TABLE );
		m_pBuffer->PutPtr( pTable );

		if ( pTable->_uiRef & MARK_FLAG )
			return;
		pTable->_uiRef |= MARK_FLAG;

		m_pBuffer->PutInt( pTable->_delegate != NULL );
		if ( pTable->_delegate )
		{
			WriteObject( pTable->_delegate );
		}

		int len = pTable->CountUsed();
		m_pBuffer->PutInt( len );

		SQObjectPtr out_key, out_val;

		while ( pTable->Next( true, NULL_HSQOBJECT, out_key, out_val ) )
		{
			WriteObject( out_key );
			WriteObject( out_val );
		}
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void WriteClass( SQClass *pClass )
	{
		m_pBuffer->PutInt( OT_CLASS );
		m_pBuffer->PutPtr( pClass );

		if ( !pClass || ( pClass->_uiRef & MARK_FLAG ) )
			return;
		pClass->_uiRef |= MARK_FLAG;

		bool bIsNative = ( pClass->_typetag != NULL );
		unsigned i;
		if ( !bIsNative )
		{
			for( i = 0; i < pClass->_methods.size(); i++) 
			{
				if ( sq_isnativeclosure( pClass->_methods[i].val ) )
				{
					bIsNative = true;
					break;
				}
			}
		}
		m_pBuffer->PutInt( bIsNative );
		if ( !bIsNative )
		{
			m_pBuffer->PutInt( pClass->_base != nullptr );
			if ( pClass->_base )
			{
				WriteObject( pClass->_base );
			}

			WriteObject( pClass->_members );
			WriteObject( pClass->_attributes );
			m_pBuffer->PutInt( pClass->_defaultvalues.size() );
			for( i = 0; i< pClass->_defaultvalues.size(); i++) 
			{
				WriteObject(pClass->_defaultvalues[i].val);
				WriteObject(pClass->_defaultvalues[i].attrs);
			}
			m_pBuffer->PutInt( pClass->_methods.size() );
			for( i = 0; i < pClass->_methods.size(); i++) 
			{
				WriteObject(pClass->_methods[i].val);
				WriteObject(pClass->_methods[i].attrs);
			}
			m_pBuffer->PutInt( std::size( pClass->_metamethods ) );
			for( i = 0; i < std::size( pClass->_metamethods ); i++) 
			{
				WriteObject(pClass->_metamethods[i]);
			}
		}
		else
		{
			if ( pClass->_typetag )
			{
				if ( pClass->_typetag == TYPETAG_VECTOR )
				{
					m_pBuffer->PutString( TYPENAME_VECTOR );
				}
				else
				{
					auto *pDesc = (ScriptClassDesc_t *)pClass->_typetag;
					m_pBuffer->PutString( pDesc->m_pszScriptName );
				}
			}
			else
			{
				// Have to grovel for the name
				SQObjectPtr key;
				if ( FindKeyForObject( m_hVM->_roottable, pClass, key ) )
				{
					m_pBuffer->PutString( key._unVal.pString->_val );
				}
				else
				{
					Assert( 0 );
					m_pBuffer->PutString( "" );
				}
			}
		}
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void WriteInstance( SQInstance *pInstance )
	{
		m_pBuffer->PutInt( OT_INSTANCE );
		m_pBuffer->PutPtr( pInstance );

		if ( pInstance->_uiRef & MARK_FLAG )
			return;
		pInstance->_uiRef |= MARK_FLAG;

		WriteObject( pInstance->_class );

		unsigned nvalues = pInstance->_class->_defaultvalues.size();
		m_pBuffer->PutInt( nvalues );
		for ( unsigned i =0; i< nvalues; i++ ) 
		{
			WriteObject( pInstance->_values[i] );
		}

		m_pBuffer->PutPtr( pInstance->_class->_typetag );

		if ( pInstance->_class->_typetag )
		{
			if ( pInstance->_class->_typetag == TYPETAG_VECTOR )
			{
				auto *pVector = (Vector *)pInstance->_userpointer;
				m_pBuffer->PutFloat( pVector->x );
				m_pBuffer->PutFloat( pVector->y );
				m_pBuffer->PutFloat( pVector->z );
			}
			else
			{
				InstanceContext_t *pContext = ((InstanceContext_t *)pInstance->_userpointer);
				WriteObject( pContext->name );
				m_pBuffer->PutPtr( pContext->pInstance );
			}
		}
		else
		{
			WriteUserPointer( NULL );
		}
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void WriteGenerator( SQGenerator *pGenerator )
	{
		ExecuteOnce( DMsg( "vscript:squirrel", 0, "Save load of generators not well tested. caveat emptor\n" ) );
		WriteObject(pGenerator->_closure);

		m_pBuffer->PutInt( OT_GENERATOR );
		m_pBuffer->PutPtr( pGenerator );

		if ( pGenerator->_uiRef & MARK_FLAG )
			return;
		pGenerator->_uiRef |= MARK_FLAG;
		
		WriteObject( pGenerator->_closure );
		m_pBuffer->PutUnsignedInt( pGenerator->_stack.size() );
		for(SQUnsignedInteger i = 0; i < pGenerator->_stack.size(); i++)
			WriteObject(pGenerator->_stack[i]);
		//m_pBuffer->PutInt( pGenerator->_vargsstack.size() );
		//for(SQUnsignedInteger j = 0; j < pGenerator->_vargsstack.size(); j++)
		//	WriteObject(pGenerator->_vargsstack[j]);
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void WriteClosure( SQClosure *pClosure )
	{
		m_pBuffer->PutInt( OT_CLOSURE );
		m_pBuffer->PutPtr( pClosure );
		if ( pClosure->_uiRef & MARK_FLAG )
			return;
		pClosure->_uiRef |= MARK_FLAG;

		WriteObject( pClosure->_function );
		WriteObject( pClosure->_env );
		WriteObject( pClosure->_root );

		m_pBuffer->PutInt( pClosure->_function->_noutervalues );
		for(SQInteger i = 0; i < pClosure->_function->_noutervalues; i++)
			WriteObject(pClosure->_outervalues[i]);
		m_pBuffer->PutInt( pClosure->_function->_ndefaultparams );
		for(SQInteger i = 0; i < pClosure->_function->_ndefaultparams; i++)
			WriteObject(pClosure->_defaultparams[i]);
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void WriteNativeClosure( SQNativeClosure *pNativeClosure )
	{
		m_pBuffer->PutInt( OT_NATIVECLOSURE );
		m_pBuffer->PutPtr( pNativeClosure );

		if ( pNativeClosure->_uiRef & MARK_FLAG )
			return;
		pNativeClosure->_uiRef |= MARK_FLAG;

		WriteObject( pNativeClosure->_name );
		return;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void WriteUserData( SQUserData *pUserData )
	{
		m_pBuffer->PutInt( OT_USERDATA );
		m_pBuffer->PutPtr( pUserData );

		if ( pUserData->_uiRef & MARK_FLAG )
			return;
		pUserData->_uiRef |= MARK_FLAG;
		
		// Need to call back or something. Unsure, TBD. [4/3/2008 tom]
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void WriteUserPointer( SQUserPointer pUserPointer )
	{
		m_pBuffer->PutInt( OT_USERPOINTER );
		// Need to call back or something. Unsure, TBD. [4/3/2008 tom]
		m_pBuffer->PutPtr( pUserPointer );
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	static SQInteger SqWriteFunc(SQUserPointer up,SQUserPointer data, SQInteger size)
	{
		auto *pThis = (CSquirrelVM *)up;
		pThis->m_pBuffer->Put( data, size );
		return size;
	}

	void WriteFuncProto( SQFunctionProto *pFuncProto )
	{
		m_pBuffer->PutInt( OT_FUNCPROTO );
		m_pBuffer->PutPtr( pFuncProto );

		// Using the map to track these as they're not collectables
		if ( m_PtrMap.Find( pFuncProto ) != m_PtrMap.InvalidIndex() )
		{
			return;
		}
		m_PtrMap.Insert( pFuncProto, pFuncProto );

		pFuncProto->Save( m_hVM, this, &SqWriteFunc );
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void WriteWeakRef( SQWeakRef *pWeakRef )
	{
		m_pBuffer->PutInt( OT_WEAKREF );
		WriteObject( pWeakRef->_obj );
	}

	//--------------------------------------------------------
	template <typename T>
	bool BeginRead( T **ppOld, T **ppNew )
	{
		*ppOld = (T *)m_pBuffer->GetPtr();
		if ( *ppOld )
		{
			auto iNew = m_PtrMap.Find( *ppOld );
			if ( iNew != m_PtrMap.InvalidIndex() )
			{
				*ppNew = (T*)m_PtrMap[iNew];
				return false;
			}
		}
		*ppNew = NULL;
		return true;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void MapPtr( void *pOld, void *pNew )
	{
		Assert( m_PtrMap.Find( pOld ) == m_PtrMap.InvalidIndex() );
		m_PtrMap.Insert( pOld, pNew );
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	bool ReadObject( SQObjectPtr &objectOut, const char *pszName = nullptr )
	{
		SQObject object;
		bool bResult = true;
		object._type = (SQObjectType)m_pBuffer->GetInt();
		if ( _RAW_TYPE(object._type) < _RT_TABLE )
		{
			switch ( object._type )
			{
			case OT_NULL:
				object._unVal.pUserPointer = 0;
				break;
			case OT_INTEGER:
				object._unVal.nInteger = m_pBuffer->GetInt();
				break;
			case OT_FLOAT:
				object._unVal.fFloat = m_pBuffer->GetFloat();
				break;
			case OT_BOOL:			
				object._unVal.nInteger = m_pBuffer->GetInt();
				break;
			case OT_STRING:
				{
					const int len = m_pBuffer->GetInt();
					char *pString = (char *)stackalloc( len + 1 );
					int read = m_pBuffer->GetUpTo( pString, len + 1 );
					if (read > 0)
					{
						pString[read - 1] = 0;
					}
					object._unVal.pString = SQString::Create( m_hVM->_sharedstate, pString, len );
					break;
				}
			default:
				Assert( 0 );
				break;
			}
		}
		else
		{
			switch ( object._type )
			{
			case OT_TABLE:
				{
					object._unVal.pTable = ReadTable();
					break;
				}
			case OT_ARRAY:			
				{
					object._unVal.pArray = ReadArray();
					break;
				}
			case OT_USERDATA:
				{
					object._unVal.pUserData = ReadUserData();			
					break;
				}
			case OT_CLOSURE:
				{
					object._unVal.pClosure = ReadClosure();
					break;
				}
			case OT_NATIVECLOSURE:	
				{
					object._unVal.pNativeClosure = ReadNativeClosure();
					break;
				}
			case OT_GENERATOR:
				{
					object._unVal.pGenerator = ReadGenerator();
					break;
				}
			case OT_USERPOINTER:
				{
					object._unVal.pUserPointer = ReadUserPointer();
					break;
				}
			case OT_THREAD:			
				{
					object._unVal.pThread = ReadVM();
					break;
				}
			case OT_FUNCPROTO:
				{
					object._unVal.pFunctionProto = ReadFuncProto();
					break;
				}
			case OT_CLASS:			
				{
					object._unVal.pClass = ReadClass();			
					break;
				}
			case OT_INSTANCE:
				{
					object._unVal.pInstance = ReadInstance();
					if ( !object._unVal.pInstance )
					{
						// Look for a match in the current root table
						HSQOBJECT hExistingObject = LookupObject( pszName, NULL, false );
						if ( sq_isinstance( hExistingObject ) )
						{
							object._unVal.pInstance = hExistingObject._unVal.pInstance;	
						}
					}
					break;
				}
			case OT_WEAKREF:		
				{
					object._unVal.pWeakRef = ReadWeakRef();
					break;
				}
			default:				
				{
					object._unVal.pUserPointer = NULL;
					Assert( 0 );
				}
			}
			if ( !object._unVal.pUserPointer )
			{
				DWarning( "vscript:squirrel",
					0,
					"Failed to restore a Squirrel object of type %s\n", SQTypeToString( object._type ) );
				object._type = OT_NULL;
				bResult = false;
			}
		}

#ifdef VSQUIRREL_DEBUG_SERIALIZATION
		lastType = object._type;
		SQObjectPtr res;
		if ( ISREFCOUNTED(object._type) )
		{
			SQ_VALIDATE_REF_COUNT( object._unVal.pRefCounted );
			object._unVal.pRefCounted->_uiRef++;
		}
		m_hVM->ToString( object, res );
		if ( ISREFCOUNTED(object._type) )
		{
			object._unVal.pRefCounted->_uiRef--;
			SQ_VALIDATE_REF_COUNT( object._unVal.pRefCounted );
		}
		DMsg( "vscript:squirrel", 0, "%d: %s [%d]\n", m_pBuffer->TellGet(),  res._unVal.pString->_val, ( ISREFCOUNTED(object._type) ) ? object._unVal.pRefCounted->_uiRef : -1 );
#ifdef VSQUIRREL_DEBUG_SERIALIZATION_HEAPCHK
		_heapchk();
#endif
#endif
		objectOut = object;
		return bResult;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	SQVM *ReadVM()
	{
		SQVM *pVM = sq_newthread( m_hVM, MIN_STACK_OVERHEAD + 2 );
		m_hVM->Pop();
		return pVM;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	void ReadVM( SQVM *pVM )
	{
		unsigned i;

		ReadObject( pVM->_roottable );
		pVM->_top = m_pBuffer->GetInt();
		pVM->_stackbase  =  m_pBuffer->GetInt();
		unsigned stackSize = m_pBuffer->GetUnsignedInt();
		pVM->_stack.resize( stackSize );
		for( i = 0; i < pVM->_stack.size(); i++ ) 
		{
			ReadObject( pVM->_stack[i] );
		}
		/*stackSize = m_pBuffer->GetUnsignedInt();
		for( i = 0; i < pVM->_vargsstack.size(); i++ ) 
		{
			ReadObject( pVM->_vargsstack[i] );
		}*/
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	SQTable *ReadTable()
	{
		SQTable *pOld;
		SQTable *pTable;

		if ( !BeginRead( &pOld, &pTable ) )
		{
			return pTable;
		}

		pTable = SQTable::Create(_ss(m_hVM), 0);

		MapPtr( pOld, pTable );

		if ( m_pBuffer->GetInt() )
		{
			SQObjectPtr delegate;
			ReadObject( delegate );
			pTable->SetDelegate( delegate._unVal.pTable );
		}
		else
		{
			pTable->_delegate = NULL;
		}
		int n = m_pBuffer->GetInt();
		while ( n-- )
		{
			SQObjectPtr key, value;
			ReadObject( key );
			if ( !ReadObject( value, ( key._type == OT_STRING ) ? key._unVal.pString->_val : NULL ) )
			{
				DWarning( "vscript:squirrel",
					0,
					"Failed to read Squirrel table entry %s\n",
					( key._type == OT_STRING ) ? key._unVal.pString->_val : SQTypeToString( key._type ) );
			}
			if ( key._type != OT_NULL )
			{
				pTable->NewSlot( key, value );
			}
		}
		return pTable;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	SQArray *ReadArray()
	{
		SQArray *pOld;
		SQArray *pArray;
		if ( !BeginRead( &pOld, &pArray ) )
		{
			return pArray;
		}

		pArray = SQArray::Create(_ss(m_hVM), 0);

		MapPtr( pOld, pArray );

		int n = m_pBuffer->GetInt();
		pArray->Reserve( n );

		while ( n-- )
		{
			SQObjectPtr value;
			ReadObject( value );
			pArray->Append( value );
		}
		return pArray;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	SQClass *ReadClass()
	{
		SQClass *pOld;
		{
			SQClass *pClass;
			if ( !BeginRead( &pOld, &pClass ) )
			{
				return pClass;
			}
		}

		SQClass *pBase = nullptr;
 		bool bIsNative = !!m_pBuffer->GetInt();
		// If it's not a C++ defined type...
		if ( !bIsNative )
		{
			if ( m_pBuffer->GetInt() )
			{
				SQObjectPtr base;
				ReadObject( base );
				pBase = base._unVal.pClass;
			}

			SQClass *pClass = SQClass::Create( _ss(m_hVM), pBase );
			MapPtr( pOld, pClass );

			SQObjectPtr members;
			ReadObject( members );
			pClass->_members->Release();
			pClass->_members = members._unVal.pTable;
			__ObjAddRef( members._unVal.pTable );

			ReadObject( pClass->_attributes );
			unsigned i, n;

			n = m_pBuffer->GetUnsignedInt();
			pClass->_defaultvalues.resize( n );
			for ( i = 0; i < n; i++ ) 
			{
				ReadObject(pClass->_defaultvalues[i].val);
				ReadObject(pClass->_defaultvalues[i].attrs);
			}

			n = m_pBuffer->GetUnsignedInt();
			pClass->_methods.resize( n );
			for ( i = 0; i < n; i++ ) 
			{
				ReadObject(pClass->_methods[i].val);
				ReadObject(pClass->_methods[i].attrs);
			}

			n = m_pBuffer->GetUnsignedInt();
			for ( i = 0; i < n; i++ ) 
			{
				ReadObject(pClass->_metamethods[i]);
			}
			return pClass;
		}
		else
		{
			char pszName[1024];
			m_pBuffer->GetString( pszName );

			SQObjectPtr value;
			if ( m_hVM->_roottable._unVal.pTable->Get( SQString::Create( _ss(m_hVM ), pszName ), value ) && sq_isclass( value ) )
			{
				MapPtr( pOld, value._unVal.pClass );
				return value._unVal.pClass;
			}
			MapPtr( pOld, nullptr );
		}
		return nullptr;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	SQInstance *ReadInstance()
	{
		SQInstance *pOld;
		SQInstance *pInstance;
		if ( !BeginRead( &pOld, &pInstance ) )
		{
			return pInstance;
		}

		SQObjectPtr pClass;
		ReadObject( pClass );

		unsigned i, n;
		if ( pClass._unVal.pClass )
		{
			pInstance = SQInstance::Create( _ss(m_hVM), pClass._unVal.pClass );

			n = m_pBuffer->GetUnsignedInt();
			for ( i = 0; i < n; i++ ) 
			{
				ReadObject(pInstance->_values[i]);
			}
			(void)m_pBuffer->GetPtr(); // ignored in this path
			if ( pInstance->_class->_typetag )
			{
				if ( pInstance->_class->_typetag == TYPETAG_VECTOR )
				{
					auto *pValue = new Vector;
					pValue->x = m_pBuffer->GetFloat();
					pValue->y = m_pBuffer->GetFloat();
					pValue->z = m_pBuffer->GetFloat();
					pInstance->_userpointer = pValue;
				}
				else
				{
					auto *pContext = new InstanceContext_t;
					pContext->pInstance = nullptr;
					ReadObject( pContext->name );
					pContext->pClassDesc = (ScriptClassDesc_t *)( pInstance->_class->_typetag );
					void *pOldInstance = m_pBuffer->GetPtr();
					if ( sq_isstring(pContext->name) )
					{
						char *pszName = pContext->name._unVal.pString->_val;
						if ( pContext->pClassDesc->pHelper )
						{
							HSQOBJECT *pInstanceHandle = new HSQOBJECT;
							pInstanceHandle->_type = OT_INSTANCE;
							pInstanceHandle->_unVal.pInstance = pInstance;
							pContext->pInstance = pContext->pClassDesc->pHelper->BindOnRead( (HSCRIPT)pInstanceHandle, pOldInstance, pszName );
							if ( pContext->pInstance )
							{
								///SQ_VALIDATE_REF_COUNT( pInstance );
								pInstance->_uiRef++;
								sq_addref( m_hVM, pInstanceHandle );
								pInstance->_uiRef--;
								//SQ_VALIDATE_REF_COUNT( pInstance );
							}
							else
							{
								delete pInstanceHandle;
							}
						}

						if ( !pContext->pInstance )
						{
							// Look for a match in the current root table
							HSQOBJECT hExistingObject = LookupObject( pszName, NULL, false );
							if ( sq_isinstance(hExistingObject) && hExistingObject._unVal.pInstance->_class == pInstance->_class )
							{
								delete pInstance;
								return hExistingObject._unVal.pInstance;	
							}

							pContext->pInstance = nullptr;
						}
					}
					pInstance->_userpointer = pContext;
				}
			}
			else 
			{
				Verify( m_pBuffer->GetInt() == OT_USERPOINTER );
				pInstance->_userpointer = ReadUserPointer();
				Assert( pInstance->_userpointer == NULL );
			}

			MapPtr( pOld, pInstance );
		}
		else
		{
			MapPtr( pOld, nullptr );
			n = m_pBuffer->GetUnsignedInt();
			for ( i = 0; i < n; i++ ) 
			{
				SQObjectPtr ignored;
				ReadObject(ignored);
			}
			void *pOldTypeTag = m_pBuffer->GetPtr(); // ignored in this path

			if ( pOldTypeTag )
			{
				if ( pOldTypeTag == TYPETAG_VECTOR )
				{
					(void)m_pBuffer->GetFloat();
					(void)m_pBuffer->GetFloat();
					(void)m_pBuffer->GetFloat();
				}
				else
				{
					SQObjectPtr ignored;
					ReadObject( ignored );
					(void)m_pBuffer->GetPtr();
				}
			}
			else 
			{
				Verify( m_pBuffer->GetInt() == OT_USERPOINTER );
				ReadUserPointer();
			}
			pInstance = nullptr;
		}
		return pInstance;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	SQGenerator *ReadGenerator()
	{
		SQGenerator *pOld;
		SQGenerator *pGenerator;
		if ( !BeginRead( &pOld, &pGenerator ) )
		{
			return pGenerator;
		}

		SQObjectPtr closure;
		ReadObject( closure );

		pGenerator = SQGenerator::Create( _ss(m_hVM), closure._unVal.pClosure );
		MapPtr( pOld, pGenerator );

		unsigned i, n;
		n = m_pBuffer->GetUnsignedInt();
		pGenerator->_stack.resize( n );
		for ( i = 0; i < n; i++ ) 
		{
			ReadObject(pGenerator->_stack[i]);
		}
		/*n = m_pBuffer->GetUnsignedInt();
		pGenerator->_vargsstack.resize( n );
		for ( i = 0; i < n; i++ ) 
		{
			ReadObject(pGenerator->_vargsstack[i]);
		}*/
		return pGenerator;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	SQClosure *ReadClosure()
	{
		SQClosure *pOld;
		SQClosure *pClosure;
		if ( !BeginRead( &pOld, &pClosure ) )
		{
			return pClosure;
		}

		SQObjectPtr proto;
		ReadObject( proto );

		SQObjectPtr env;
		ReadObject( env );
		
		SQObjectPtr root;
		ReadObject( root );

		pClosure = SQClosure::Create( _ss(m_hVM), proto._unVal.pFunctionProto, root._unVal.pWeakRef );
		MapPtr( pOld, pClosure );

		pClosure->_env = env._unVal.pWeakRef;

		int n = m_pBuffer->GetInt();
		pClosure->_function->_noutervalues = n;
		for ( int i = 0; i < n; i++ ) 
		{
			ReadObject(pClosure->_outervalues[i]);
		}

		n = m_pBuffer->GetInt();
		pClosure->_function->_ndefaultparams = n;
		for ( int i = 0; i < n; i++ ) 
		{
			ReadObject(pClosure->_defaultparams[i]);
		}

		return pClosure;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	SQNativeClosure *ReadNativeClosure()
	{
		SQNativeClosure *pOld;
		SQNativeClosure *pClosure;
		if ( !BeginRead( &pOld, &pClosure ) )
		{
			return pClosure;
		}

		SQObjectPtr name;
		ReadObject( name );
		SQObjectPtr value;
		if ( m_hVM->_roottable._unVal.pTable->Get( name, value ) && sq_isnativeclosure(value) )
		{
			MapPtr( pOld, value._unVal.pNativeClosure );
			return value._unVal.pNativeClosure;
		}
		MapPtr( pOld, nullptr );
		return nullptr; // @TBD [4/15/2008 tom]
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	SQUserData *ReadUserData()
	{
		return static_cast<SQUserData *>(m_pBuffer->GetPtr()); // @TBD [4/15/2008 tom]
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	SQUserPointer ReadUserPointer()
	{
		return m_pBuffer->GetPtr(); // @TBD [4/15/2008 tom]
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	static SQInteger SqReadFunc(SQUserPointer up,SQUserPointer data, SQInteger size)
 	{
 		auto *pThis = (CSquirrelVM *)up;
 		pThis->m_pBuffer->Get( data, size );
 		return size;
 	}
 
	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	SQFunctionProto *ReadFuncProto()
	{
		SQFunctionProto *pOld;
		SQFunctionProto *pResult;
		if ( !BeginRead( &pOld, &pResult ) )
		{
			return pResult;
		}

		SQObjectPtr result;
		SQFunctionProto::Load( m_hVM, this, &SqReadFunc, result );
		pResult = result._unVal.pFunctionProto;
		//SQ_VALIDATE_REF_COUNT( pResult );
		pResult->_uiRef++;
		result.Null();
		pResult->_uiRef--;
		//SQ_VALIDATE_REF_COUNT( pResult );
		MapPtr( pOld, pResult );
		return pResult;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	SQWeakRef *ReadWeakRef( )
	{
		SQObjectPtr obj;
		ReadObject( obj );
		if ( !obj._unVal.pRefCounted )
		{
			return nullptr;
		}

		// Need to up ref count if read order has weak ref loading first
		Assert( ISREFCOUNTED(obj._type) );
		__AddRef(obj._type, obj._unVal);

		SQWeakRef *pResult = obj._unVal.pRefCounted->GetWeakRef( obj._type );

		obj.Null();
		__Release(obj._type, obj._unVal);

		return pResult;
	}

	//-------------------------------------------------------------
	//
	//-------------------------------------------------------------
	bool FindKeyForObject( const SQObjectPtr &table, void *p, SQObjectPtr &key )
	{
		SQTable *pTable = table._unVal.pTable;

		SQObjectPtr out_key, out_val;

		while ( pTable->Next( true, NULL_HSQOBJECT, out_key, out_val ) )
		{
			if ( out_val._unVal.pUserPointer == p )
			{
				key = out_key;
				return true;
			}

			if ( sq_istable( out_val ) )
			{
				if ( FindKeyForObject( out_val, p, key ) )
				{
					return true;
				}
			}
		}
		return false;
	}

	//-------------------------------------------------------------------------
	// 
	//-------------------------------------------------------------------------

	HSQUIRRELVM		m_hVM;
	sq::dbg::HSQREMOTEDBG	m_hDbg;
	HSQOBJECT		m_hOnCreateScopeFunc;
	HSQOBJECT		m_hOnReleaseScopeFunc;
	HSQOBJECT		m_hClassVector;
	SQObjectPtr		m_ErrorString;
	uint64			m_iUniqueIdSerialNumber;
	// dimhotepus: float -> double.
	double			m_TimeStartExecute;
#ifdef VSQUIRREL_TEST
	ConVar			developer;
#else
	ConVarRef		developer;
#endif

	CUtlHashFast<SQClass *, CUtlHashFastGenericHash> m_TypeMap;
	friend class CVSquirrelSerializer;

	// Serialization support
	CUtlBuffer *m_pBuffer;
	CUtlMap<void *, void *> m_PtrMap;
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

IScriptVM *ScriptCreateSquirrelVM()
{
	return new CSquirrelVM;
}

void ScriptDestroySquirrelVM( IScriptVM *pVM )
{
	CSquirrelVM *pSquirrelVM = assert_cast<CSquirrelVM *>( pVM );
	delete pSquirrelVM;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#ifdef VSQUIRREL_TEST

#include <ctime>
#include <conio.h>

#include "posix_file_stream.h"
#include "tier0/fasttimer.h"

CSquirrelVM g_SquirrelVM;
IScriptVM *g_pScriptVM = &g_SquirrelVM;


static void FromScript_AddBehavior( const char *pBehaviorName, HSCRIPT hTable )
{
	ScriptVariant_t		KeyVariant, ValueVariant;

	Msg( "Behavior: %s\n", pBehaviorName );

	int nInterator = 0;
	int index =	g_pScriptVM->GetNumTableEntries( hTable );
	for( int i = 0; i < index; i++ )
	{
		nInterator = g_pScriptVM->GetKeyValue( hTable, nInterator, &KeyVariant, &ValueVariant );

		Msg( "   %d: %s / %s\n", i, static_cast<const char*>(KeyVariant), static_cast<const char*>(ValueVariant) );

		g_pScriptVM->ReleaseValue( KeyVariant );
		g_pScriptVM->ReleaseValue( ValueVariant );
	}
}

static ScriptVariant_t TestReturn( )
{
	return ScriptVariant_t("test");
}

static Vector MyVectorAdd( Vector A, Vector B )
{
	return A + B;
}

void TestOutput( const char *pszText )
{
	Msg( "%s\n", pszText );
}

bool TestError( const char *pszText )
{
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
	ScriptVariant_t MyValue = 10;
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
}

float CMyClass::FooBar( int test1, const char *test2 )
{
	return 2.34f;
}

float CMyClass::OverlyTechnicalName( bool test )
{
	return 4.56f;
}

BEGIN_SCRIPTDESC_ROOT_NAMED( CMyClass, "CMyClass", SCRIPT_SINGLETON "" )
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

	if ( !g_pScriptVM->Init() )
	{
		fprintf( stderr, "Squirrel VM init failure.\n" );
		return EINVAL;
	}

	g_pScriptVM->SetOutputCallback( TestOutput );

	AnotherFunction();

	CCycleCount count;
	count.Sample();
	RandomSeed( time( NULL ) ^ count.GetMicroseconds() );
	ScriptRegisterFunction( g_pScriptVM, RandomFloat, "" );
	ScriptRegisterFunction( g_pScriptVM, RandomInt, "" );

	ScriptRegisterFunction( g_pScriptVM, FromScript_AddBehavior, "" );
	ScriptRegisterFunction( g_pScriptVM, MyVectorAdd, "" );
	ScriptRegisterFunction( g_pScriptVM, TestReturn, "" );

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
		
		const auto fileLenTyped = static_cast<unsigned>( nFileLen );
		std::unique_ptr<char[]> pBuf = std::make_unique<char[]>(fileLenTyped + 1);
		std::tie(std::ignore, rc) = hFile.read( pBuf.get(), fileLenTyped, 1, fileLenTyped );
		pBuf[fileLenTyped] = '\0';

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
			Msg( "%zu bytes.\n", g_pMemAlloc->GetSize( NULL ) );
		}

		if ( key == 'r' )
		{
			scope.Term();
			scope.Init( "TestScope" );
		}
	} while ( key != 'q' );

	scope.Term();
	g_pScriptVM->DisconnectDebugger();

	g_pScriptVM->Shutdown();
	return 0;
}

#endif
