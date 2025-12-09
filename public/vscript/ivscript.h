//========== Copyright Valve Corporation, All rights reserved. ========
//
// Purpose: VScript
//
// Overview
// --------
// VScript is an abstract binding layer that allows code to expose itself to 
// multiple scripting languages in a uniform format. Code can expose 
// functions, classes, and data to the scripting languages, and can also 
// call functions that reside in scripts.
// 
// Initializing
// ------------
// 
// To create a script virtual machine (VM), grab the global instance of 
// IScriptManager, call CreateVM, then call Init on the returned VM. Right 
// now you can have multiple VMs, but only VMs for a specific language.
// 
// Exposing functions and classes
// ------------------------------
// 
// To expose a C++ function to the scripting system, you just need to fill out a 
// description block. Using templates, the system will automatically deduce 
// all of the binding requirements (parameters and return values). Functions 
// are limited as to what the types of the parameters can be. See ScriptVariant_t.
// 
// 		extern IScriptVM *pScriptVM;
// 		bool Foo( int );
// 		void Bar();
// 		float FooBar( int, const char * );
// 		float OverlyTechnicalName( bool );
// 
// 		void RegisterFuncs()
// 		{
// 			ScriptRegisterFunction( pScriptVM, Foo );
// 			ScriptRegisterFunction( pScriptVM, Bar );
// 			ScriptRegisterFunction( pScriptVM, FooBar );
// 			ScriptRegisterFunctionNamed( pScriptVM, OverlyTechnicalName, "SimpleName" );
// 		}
// 
// 		class CMyClass
// 		{
// 		public:
// 			bool Foo( int );
// 			void Bar();
// 			float FooBar( int, const char * );
// 			float OverlyTechnicalName( bool );
// 		};
// 
// 		BEGIN_SCRIPTDESC_ROOT( CMyClass )
// 			DEFINE_SCRIPTFUNC( Foo )
// 			DEFINE_SCRIPTFUNC( Bar )
// 			DEFINE_SCRIPTFUNC( FooBar )
// 			DEFINE_SCRIPTFUNC_NAMED( OverlyTechnicalName, "SimpleMemberName" )
// 		END_SCRIPTDESC();
// 
// 		class CMyDerivedClass : public CMyClass
// 		{
// 		public:
// 			float DerivedFunc() const;
// 		};
// 
// 		BEGIN_SCRIPTDESC( CMyDerivedClass, CMyClass )
// 			DEFINE_SCRIPTFUNC( DerivedFunc )
// 		END_SCRIPTDESC();
// 
// 		CMyDerivedClass derivedInstance;
// 
// 		void AnotherFunction()
// 		{
// 			// Manual class exposure
// 			pScriptVM->RegisterClass( GetScriptDescForClass( CMyClass ) );
// 
// 			// Auto registration by instance
// 			pScriptVM->RegisterInstance( &derivedInstance, "theInstance" );
// 		}
// 
// Classes with "DEFINE_SCRIPT_CONSTRUCTOR()" in their description can be instanced within scripts
//
// Scopes
// ------
// Scripts can either be run at the global scope, or in a user defined scope. In the latter case,
// all "globals" within the script are actually in the scope. This can be used to bind private
// data spaces with C++ objects.
//
// Calling a function on a script
// ------------------------------
// Generally, use the "Call" functions. This example is the equivalent of DoIt("Har", 6.0, 99).
//
// 		hFunction = pScriptVM->LookupFunction( "DoIt", hScope );
// 		pScriptVM->Call( hFunction, hScope, true, NULL, "Har", 6.0, 99 );
// 
//=============================================================================

#ifndef IVSCRIPT_H
#define IVSCRIPT_H

#include "datamap.h"
#include "appframework/IAppSystem.h"
#include "tier0/platform.h"
#include "tier1/functors.h"
#include "vscript/variant.h"
#include <functional>
#include "tier0/memdbgon.h"

#ifdef VSCRIPT_DLL_EXPORT
#define VSCRIPT_INTERFACE	DLL_EXPORT
#define VSCRIPT_OVERLOAD	DLL_GLOBAL_EXPORT
#define VSCRIPT_CLASS		DLL_CLASS_EXPORT
#else
#define VSCRIPT_INTERFACE	DLL_IMPORT
#define VSCRIPT_OVERLOAD	DLL_GLOBAL_IMPORT
#define VSCRIPT_CLASS		DLL_CLASS_IMPORT
#endif

class CUtlBuffer;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

constexpr inline char VSCRIPT_INTERFACE_VERSION[]{"VScriptManager010"};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

class IScriptVM;

enum ScriptLanguage_t
{
	SL_NONE,
	SL_GAMEMONKEY,
	SL_SQUIRREL,
	SL_LUA,
	SL_PYTHON,

	SL_DEFAULT = SL_SQUIRREL
};

abstract_class IScriptManager : public IAppSystem
{
public:
	virtual IScriptVM *CreateVM( ScriptLanguage_t language = SL_DEFAULT ) = 0;
	virtual void DestroyVM( IScriptVM * ) = 0;
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

DECLARE_POINTER_HANDLE( HSCRIPT );
#define INVALID_HSCRIPT ((HSCRIPT)-1)

inline bool IsValid( HSCRIPT hScript )
{
	return ( hScript != nullptr && hScript != INVALID_HSCRIPT );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

using ScriptDataType_t = int;
using ScriptVariant_t = CVariant;
#define SCRIPT_VARIANT_NULL VARIANT_NULL
#define ScriptDeduceType( T ) VariantDeduceType( T )

template <typename T>
inline const char * ScriptFieldTypeName() 
{
	return VariantFieldTypeName< T >();
}

inline const char * ScriptFieldTypeName( int16 eType )
{
	return VariantFieldTypeName( eType );
}

//---------------------------------------------------------

struct ScriptFuncDescriptor_t
{
	ScriptFuncDescriptor_t()
	{
		m_pszScriptName = nullptr;
		m_pszFunction = nullptr;
		m_pszDescription = nullptr;
		m_ReturnType = FIELD_TYPEUNKNOWN;
	}

	const char *m_pszScriptName;
	const char *m_pszFunction;
	const char *m_pszDescription;
	ScriptDataType_t m_ReturnType;
	CUtlVector<ScriptDataType_t> m_Parameters;
};


//---------------------------------------------------------

// Prefix a script description with this in order to not show the function or class in help
#define SCRIPT_HIDE "@"

// Prefix a script description of a class to indicate it is a singleton and the single instance should be in the help
#define SCRIPT_SINGLETON "!"

// Prefix a script description with this to indicate it should be represented using an alternate name
#define SCRIPT_ALIAS( alias, description ) "#" alias ":" description

//---------------------------------------------------------

enum ScriptFuncBindingFlags_t
{
	SF_MEMBER_FUNC	= 0x01,
};

struct ScriptFunctionBindingStorageType_t
{
	intptr_t val_0;
	intptr_t val_1;
	// Josh:
	// Why do we need *even more* space for a function pointer?
	// MSVC is very special.
	// Read https://rants.vastheman.com/2021/09/21/msvc/
	// This accounts for the "Unknown Inheritance" case, which
	// CTFPlayer hits.
	intptr_t val_2;
	intptr_t val_3;
};

using ScriptBindingFunc_t = bool (*)(ScriptFunctionBindingStorageType_t, void *, ScriptVariant_t *, intp, ScriptVariant_t *);

struct ScriptFunctionBinding_t
{
	ScriptFuncDescriptor_t	m_desc;
	ScriptBindingFunc_t		m_pfnBinding;
	ScriptFunctionBindingStorageType_t	m_pFunction;
	unsigned				m_flags;
};

//---------------------------------------------------------
class IScriptInstanceHelper
{
public:
	virtual void *GetProxied( void *p, ScriptFunctionBinding_t *pBinding )			{ return p; }
	virtual bool ToString( void *p, char *pBuf, int bufSize )						{ return false; }
	virtual void *BindOnRead( HSCRIPT hInstance, void *pOld, const char *pszId )	{ return nullptr; }
};

//---------------------------------------------------------

struct ScriptClassDesc_t
{
	explicit ScriptClassDesc_t( void (*pfnInitializer)() )
	{
		(*pfnInitializer)();
		ScriptClassDesc_t **ppHead = GetDescList();
		m_pNextDesc = *ppHead;
		*ppHead = this;
	}

	const char *						m_pszScriptName{nullptr};
	const char *						m_pszClassname{nullptr};
	const char *						m_pszDescription{nullptr};
	ScriptClassDesc_t *					m_pBaseDesc{nullptr};
	CUtlVector<ScriptFunctionBinding_t> m_FunctionBindings;

	void *(*m_pfnConstruct)(){nullptr};
	void (*m_pfnDestruct)( void *){nullptr};
	IScriptInstanceHelper *				pHelper{nullptr}; // optional helper

	ScriptClassDesc_t *					m_pNextDesc;

	static ScriptClassDesc_t **GetDescList()
	{
		static ScriptClassDesc_t *pHead;
		return &pHead;
	}
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

#include "vscript_templates.h"

// Lower level macro primitives
#define ScriptInitFunctionBinding( pScriptFunction, func )									ScriptInitFunctionBindingNamed( pScriptFunction, func, #func )
#define ScriptInitFunctionBindingNamed( pScriptFunction, func, scriptName )					do { ScriptInitFuncDescriptorNamed( (&(pScriptFunction)->m_desc), func, scriptName ); (pScriptFunction)->m_pfnBinding = ScriptCreateBinding( &func ); (pScriptFunction)->m_pFunction = ScriptConvertFreeFuncPtrToVoid( &func ); } while (0)

#define ScriptInitMemberFunctionBinding( pScriptFunction, class, func )						ScriptInitMemberFunctionBinding_( pScriptFunction, class, func, #func )
#define ScriptInitMemberFunctionBindingNamed( pScriptFunction, class, func, scriptName )	ScriptInitMemberFunctionBinding_( pScriptFunction, class, func, scriptName )
#define ScriptInitMemberFunctionBinding_( pScriptFunction, class, func, scriptName ) 		do { ScriptInitMemberFuncDescriptor_( (&(pScriptFunction)->m_desc), class, func, scriptName ); (pScriptFunction)->m_pfnBinding = ScriptCreateBinding( ((class *)0), &class::func ); 	(pScriptFunction)->m_pFunction = ScriptConvertFuncPtrToVoid( &class::func ); (pScriptFunction)->m_flags = SF_MEMBER_FUNC;  } while (0)

#define ScriptInitClassDesc( pClassDesc, class, pBaseClassDesc )							ScriptInitClassDescNamed( pClassDesc, class, pBaseClassDesc, #class )
#define ScriptInitClassDescNamed( pClassDesc, class, pBaseClassDesc, scriptName )			ScriptInitClassDescNamed_( pClassDesc, class, pBaseClassDesc, scriptName )
#define ScriptInitClassDescNoBase( pClassDesc, class )										ScriptInitClassDescNoBaseNamed( pClassDesc, class, #class )
#define ScriptInitClassDescNoBaseNamed( pClassDesc, class, scriptName )						ScriptInitClassDescNamed_( pClassDesc, class, NULL, scriptName )
#define ScriptInitClassDescNamed_( pClassDesc, class, pBaseClassDesc, scriptName )			do { (pClassDesc)->m_pszScriptName = scriptName; (pClassDesc)->m_pszClassname = #class; (pClassDesc)->m_pBaseDesc = pBaseClassDesc; } while ( 0 )

#define ScriptAddFunctionToClassDesc( pClassDesc, class, func, description  )				ScriptAddFunctionToClassDescNamed( pClassDesc, class, func, #func, description )
#define ScriptAddFunctionToClassDescNamed( pClassDesc, class, func, scriptName, description ) do { ScriptFunctionBinding_t *pBinding = &((pClassDesc)->m_FunctionBindings[(pClassDesc)->m_FunctionBindings.AddToTail()]); pBinding->m_desc.m_pszDescription = description; ScriptInitMemberFunctionBindingNamed( pBinding, class, func, scriptName );  } while (0)

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

#define ScriptRegisterFunction( pVM, func, description )									ScriptRegisterFunctionNamed( pVM, func, #func, description )
#define ScriptRegisterFunctionNamed( pVM, func, scriptName, description )					do { static ScriptFunctionBinding_t binding; binding.m_desc.m_pszDescription = description; binding.m_desc.m_Parameters.RemoveAll(); ScriptInitFunctionBindingNamed( &binding, func, scriptName ); pVM->RegisterFunction( &binding ); } while (0)

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

#define ALLOW_SCRIPT_ACCESS() 																template <typename T> friend ScriptClassDesc_t *GetScriptDesc(T *);

#define BEGIN_SCRIPTDESC( className, baseClass, description )								BEGIN_SCRIPTDESC_NAMED( className, baseClass, #className, description )
#define BEGIN_SCRIPTDESC_ROOT( className, description )										BEGIN_SCRIPTDESC_ROOT_NAMED( className, #className, description )

#define DEFINE_SCRIPTDESC_FUNCTION( className, baseClass ) \
	template <> ScriptClassDesc_t * GetScriptDesc<baseClass>( baseClass *); \
	template <> ScriptClassDesc_t * GetScriptDesc<className>( className *)

struct ScriptNoBase_t;

// We use template specialization to allow classes to optionally override this function.
// For a given class, if this function is NOT overridden, the class' descriptor will use the base class'
// IScriptInstanceHelper object.
// If this function IS overridden, it will use the return value of the overridden function
template < typename TScriptClass > 
IScriptInstanceHelper *GetScriptInstanceHelperOverride( IScriptInstanceHelper *pBaseClassHelper )
{
	return pBaseClassHelper;
}

inline IScriptInstanceHelper *GetScriptInstanceHelper_ScriptNoBase_t()
{
	return nullptr;
}

#define BEGIN_SCRIPTDESC_NAMED( className, baseClass, scriptName, description ) \
	IScriptInstanceHelper *GetScriptInstanceHelper_##baseClass(); \
	IScriptInstanceHelper *GetScriptInstanceHelper_##className() \
	{ \
		return GetScriptInstanceHelperOverride< className >( GetScriptInstanceHelper_##baseClass() ); \
	}; \
	extern void Init##className##ScriptDesc(); \
	ScriptClassDesc_t g_##className##_ScriptDesc( &Init##className##ScriptDesc ); \
	DEFINE_SCRIPTDESC_FUNCTION( className, baseClass ) \
	{ \
		return &g_##className##_ScriptDesc; \
	} \
	\
	void Init##className##ScriptDesc() \
	{ \
		static bool bInitialized; \
		if ( bInitialized ) \
		{ \
			return; \
		} \
		\
		bInitialized = true; \
		\
		typedef className _className; \
		ScriptClassDesc_t *pDesc = &g_##className##_ScriptDesc; \
		pDesc->m_pszDescription = description; \
		ScriptInitClassDescNamed( pDesc, className, GetScriptDescForClass( baseClass ), scriptName ); \
		pDesc->pHelper = GetScriptInstanceHelper_##className();


#define BEGIN_SCRIPTDESC_ROOT_NAMED( className, scriptName, description ) \
	BEGIN_SCRIPTDESC_NAMED( className, ScriptNoBase_t, scriptName, description )

#define END_SCRIPTDESC() \
		return; \
	}

#define SCRIPTFUNC_CONCAT_(x, y) x##y
#define SCRIPTFUNC_CONCAT(x, y) SCRIPTFUNC_CONCAT_(x, y)

#define DEFINE_SCRIPTFUNC( func, description )												DEFINE_SCRIPTFUNC_NAMED( func, #func, description )
#define DEFINE_SCRIPTFUNC_WRAPPED( func, description )										DEFINE_SCRIPTFUNC_NAMED( SCRIPTFUNC_CONCAT( Script, func ), #func, description )
#define DEFINE_SCRIPTFUNC_NAMED( func, scriptName, description )							ScriptAddFunctionToClassDescNamed( pDesc, _className, func, scriptName, description );
#define DEFINE_SCRIPT_CONSTRUCTOR()															ScriptAddConstructorToClassDesc( pDesc, _className );
#define DEFINE_SCRIPT_INSTANCE_HELPER( className, p )										template <> IScriptInstanceHelper *GetScriptInstanceHelperOverride< className >( IScriptInstanceHelper * ) { return p; }
								
template <typename T> ScriptClassDesc_t *GetScriptDesc(T *);

template <>
inline ScriptClassDesc_t *GetScriptDesc<ScriptNoBase_t>( ScriptNoBase_t *) { return nullptr; }

#define GetScriptDescForClass( className ) GetScriptDesc( ( className *)nullptr )

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

template <typename T>
class CScriptConstructor
{
public:
	static void *Construct()		{ return new T; }
	static void Destruct( void *p )	{ delete (T *)p; }
};

#define ScriptAddConstructorToClassDesc( pClassDesc, class )								do { (pClassDesc)->m_pfnConstruct = &CScriptConstructor<class>::Construct; (pClassDesc)->m_pfnDestruct = &CScriptConstructor<class>::Destruct; } while (0)

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

enum ScriptErrorLevel_t
{
	SCRIPT_LEVEL_WARNING	= 0,
	SCRIPT_LEVEL_ERROR,
};

using ScriptOutputFunc_t = void (*)(const char *);
using ScriptErrorFunc_t = bool (*)(ScriptErrorLevel_t, const char *);

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

#ifdef RegisterClass
#undef RegisterClass
#endif

enum ScriptStatus_t
{
	SCRIPT_ERROR = -1,
	SCRIPT_DONE,
	SCRIPT_RUNNING,
};

abstract_class IScriptVM
{
public:
	virtual bool Init() = 0;
	virtual void Shutdown() = 0;

	virtual bool ConnectDebugger() = 0;
	virtual void DisconnectDebugger() = 0;

	virtual ScriptLanguage_t GetLanguage() = 0;
	virtual const char *GetLanguageName() = 0;

	virtual void AddSearchPath( const char *pszSearchPath ) = 0;

	//--------------------------------------------------------
 
 	virtual bool Frame( float simTime ) = 0;

	//--------------------------------------------------------
	// Simple script usage
	//--------------------------------------------------------
	virtual ScriptStatus_t Run( const char *pszScript, bool bWait = true ) = 0;
	inline ScriptStatus_t Run( const unsigned char *pszScript, bool bWait = true ) { return Run( (const char *)pszScript, bWait ); }

	//--------------------------------------------------------
	// Compilation
	//--------------------------------------------------------
 	virtual HSCRIPT CompileScript( const char *pszScript, const char *pszId = nullptr ) = 0;
	inline HSCRIPT CompileScript( const unsigned char *pszScript, const char *pszId = nullptr ) { return CompileScript( (const char *)pszScript, pszId ); }
	virtual void ReleaseScript( HSCRIPT ) = 0;

	//--------------------------------------------------------
	// Execution of compiled
	//--------------------------------------------------------
	virtual ScriptStatus_t Run( HSCRIPT hScript, HSCRIPT hScope = nullptr, bool bWait = true ) = 0;
	virtual ScriptStatus_t Run( HSCRIPT hScript, bool bWait ) = 0;

	//--------------------------------------------------------
	// Scope
	//--------------------------------------------------------
	virtual HSCRIPT CreateScope( const char *pszScope, HSCRIPT hParent = nullptr ) = 0;
	virtual HSCRIPT ReferenceScope( HSCRIPT hScript ) = 0;
	virtual void ReleaseScope( HSCRIPT hScript ) = 0;

	//--------------------------------------------------------
	// Script functions
	//--------------------------------------------------------
	virtual HSCRIPT LookupFunction( const char *pszFunction, HSCRIPT hScope = nullptr ) = 0;
	virtual void ReleaseFunction( HSCRIPT hScript ) = 0;

	//--------------------------------------------------------
	// Script functions (raw, use Call())
	//--------------------------------------------------------
	virtual ScriptStatus_t ExecuteFunction( HSCRIPT hFunction, ScriptVariant_t *pArgs, intp nArgs, ScriptVariant_t *pReturn, HSCRIPT hScope, bool bWait ) = 0;

	//--------------------------------------------------------
	// External functions
	//--------------------------------------------------------
	virtual void RegisterFunction( ScriptFunctionBinding_t *pScriptFunction ) = 0;

	//--------------------------------------------------------
	// External classes
	//--------------------------------------------------------
	virtual bool RegisterClass( ScriptClassDesc_t *pClassDesc ) = 0;

	void RegisterAllClasses()
	{
		ScriptClassDesc_t *pCurrent = *ScriptClassDesc_t::GetDescList();
		while ( pCurrent )
		{
			RegisterClass( pCurrent );
			pCurrent = pCurrent->m_pNextDesc;
		}
	}

	//--------------------------------------------------------
	// External instances. Note class will be auto-registered.
	//--------------------------------------------------------

	virtual HSCRIPT RegisterInstance( ScriptClassDesc_t *pDesc, void *pInstance ) = 0;
	virtual void SetInstanceUniqeId( HSCRIPT hInstance, const char *pszId ) = 0;
	template <typename T> HSCRIPT RegisterInstance( T *pInstance )																	{ return RegisterInstance( GetScriptDesc( pInstance ), pInstance );	}
	template <typename T> HSCRIPT RegisterInstance( T *pInstance, const char *pszInstance, HSCRIPT hScope = nullptr)					{ HSCRIPT hInstance = RegisterInstance( GetScriptDesc( pInstance ), pInstance ); SetValue( hScope, pszInstance, hInstance ); return hInstance; }
	virtual void RemoveInstance( HSCRIPT ) = 0;
	void RemoveInstance( HSCRIPT hInstance, const char *pszInstance, HSCRIPT hScope = nullptr )										{ ClearValue( hScope, pszInstance ); RemoveInstance( hInstance ); }
	void RemoveInstance( const char *pszInstance, HSCRIPT hScope = nullptr )															{ ScriptVariant_t val; if ( GetValue( hScope, pszInstance, &val ) ) { if ( val.GetType() == FIELD_HSCRIPT ) { RemoveInstance( val, pszInstance, hScope ); } ReleaseValue( val ); } }

	virtual void *GetInstanceValue( HSCRIPT hInstance, ScriptClassDesc_t *pExpectedType = nullptr ) = 0;

	//----------------------------------------------------------------------------

	virtual bool GenerateUniqueKey( const char *pszRoot, char *pBuf, int nBufSize ) = 0;

	//----------------------------------------------------------------------------

	virtual bool ValueExists( HSCRIPT hScope, const char *pszKey ) = 0;
	bool ValueExists( const char *pszKey )																							{ return ValueExists( nullptr, pszKey ); }

	virtual bool SetValue( HSCRIPT hScope, const char *pszKey, const char *pszValue ) = 0;
	virtual bool SetValue( HSCRIPT hScope, const char *pszKey, const ScriptVariant_t &value ) = 0;
	bool SetValue( const char *pszKey, const ScriptVariant_t &value )																{ return SetValue(nullptr, pszKey, value ); }

	virtual void CreateTable( ScriptVariant_t &Table ) = 0;
	virtual intp GetNumTableEntries( HSCRIPT hScope ) = 0;
	virtual intp GetKeyValue( HSCRIPT hScope, intp nIterator, ScriptVariant_t *pKey, ScriptVariant_t *pValue ) = 0;

	virtual bool GetValue( HSCRIPT hScope, const char *pszKey, ScriptVariant_t *pValue ) = 0;
	bool GetValue( const char *pszKey, ScriptVariant_t *pValue )																	{ return GetValue(nullptr, pszKey, pValue ); }
	virtual void ReleaseValue( ScriptVariant_t &value ) = 0;

	virtual bool ClearValue( HSCRIPT hScope, const char *pszKey ) = 0;
	bool ClearValue( const char *pszKey)																							{ return ClearValue( nullptr, pszKey ); }

	//----------------------------------------------------------------------------

	// Josh: Some extra helpers here.
	template <typename T>
	T Get( HSCRIPT hScope, const char *pszKey )
	{
		ScriptVariant_t variant;
		GetValue( hScope, pszKey, &variant );
		return variant.Get<T>();
	}

	template <typename T>
	T Get( const char *pszKey )
	{
		return Get<T>( nullptr, pszKey );
	}

	bool Has( HSCRIPT hScope, const char* pszKey )
	{
		return ValueExists( hScope, pszKey );
	}

	bool Has( const char* pszKey )
	{
		return Has( nullptr, pszKey );
	}

	template <typename T>
	using IfHasFuncType = std::function<void(T)>;

	template <typename T>
	void IfHas( HSCRIPT hScope, const char *pszKey, IfHasFuncType<T> func )
	{
		if ( Has( hScope, pszKey ) )
		{
			func( Get<T>( hScope, pszKey ) );
		}
	}

	//----------------------------------------------------------------------------

	virtual void WriteState( CUtlBuffer *pBuffer ) = 0;
	virtual void ReadState( CUtlBuffer *pBuffer ) = 0;
	virtual void RemoveOrphanInstances() = 0;

	virtual void DumpState() = 0;

	virtual void SetOutputCallback( ScriptOutputFunc_t pFunc ) = 0;
	virtual void SetErrorCallback( ScriptErrorFunc_t pFunc ) = 0;

	//----------------------------------------------------------------------------

	virtual bool RaiseException( const char *pszExceptionText ) = 0;

	//----------------------------------------------------------------------------
	// Call API
	//
	// Note for string and vector return types, the caller must delete the pointed to memory
	//----------------------------------------------------------------------------
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope = nullptr, bool bWait = true, ScriptVariant_t *pReturn = nullptr )
	{
		return ExecuteFunction( hFunction, nullptr, 0, pReturn, hScope, bWait );
	}

	template <typename... TArgs>
	ScriptStatus_t Call( HSCRIPT hFunction, HSCRIPT hScope, bool bWait, ScriptVariant_t *pReturn, TArgs... args )
	{
		constexpr std::size_t size = sizeof...(TArgs);

		ScriptVariant_t vars[size] = {std::forward(args)...};
		
		return ExecuteFunction( hFunction, vars, ARRAYSIZE(vars), pReturn, hScope, bWait );
	}
};


//-----------------------------------------------------------------------------
// Script scope helper class
//-----------------------------------------------------------------------------

class CDefScriptScopeBase
{
public:
	static IScriptVM *GetVM()
	{
		extern IScriptVM *g_pScriptVM;
		return g_pScriptVM;
	}
};

template <class BASE_CLASS = CDefScriptScopeBase>
class CScriptScopeT : public CDefScriptScopeBase
{
public:
	CScriptScopeT() :
		m_hScope( INVALID_HSCRIPT ),
		m_flags( 0 )
	{
	}

	~CScriptScopeT()
	{
		Term();
	}

	bool IsInitialized()
	{
		return m_hScope != INVALID_HSCRIPT;
	}

	bool Init( const char *pszName )
	{
		m_hScope = GetVM()->CreateScope( pszName );
		return ( m_hScope != NULL );
	}

	bool Init( HSCRIPT hScope, bool bExternal = true )
	{
		if ( bExternal )
		{
			m_flags |= EXTERNAL;
		}
		m_hScope = hScope;
		return ( m_hScope != NULL );
	}

	bool InitGlobal()
	{
		Assert( 0 ); // todo [3/24/2008 tom]
		m_hScope = GetVM()->CreateScope( "" );
		return ( m_hScope != NULL );
	}

	void Term()
	{
		if ( m_hScope != INVALID_HSCRIPT )
		{
			IScriptVM *pVM = GetVM();
			if ( pVM )
			{
				for ( int i = 0; i < m_FuncHandles.Count(); i++ )
				{
					pVM->ReleaseFunction( *m_FuncHandles[i] );
				}
			}
			m_FuncHandles.Purge();
			if ( m_hScope && pVM && !(m_flags & EXTERNAL) )
			{
				pVM->ReleaseScope( m_hScope );
			}
			m_hScope = INVALID_HSCRIPT;
		}
		m_flags = 0;
	}

	void InvalidateCachedValues()
	{
		IScriptVM *pVM = GetVM();
		for ( int i = 0; i < m_FuncHandles.Count(); i++ )
		{
			if ( *m_FuncHandles[i] )
				pVM->ReleaseFunction( *m_FuncHandles[i] );
			*m_FuncHandles[i] = INVALID_HSCRIPT;
		}
		m_FuncHandles.RemoveAll();
	}

	operator HSCRIPT()
	{
		return ( m_hScope != INVALID_HSCRIPT ) ? m_hScope : NULL;
	}

	bool ValueExists( const char *pszKey )																							{ return GetVM()->ValueExists( m_hScope, pszKey ); }
	bool SetValue( const char *pszKey, const ScriptVariant_t &value )																{ return GetVM()->SetValue(m_hScope, pszKey, value ); }
	bool GetValue( const char *pszKey, ScriptVariant_t *pValue )																	{ return GetVM()->GetValue(m_hScope, pszKey, pValue ); }
	void ReleaseValue( ScriptVariant_t &value )																						{ GetVM()->ReleaseValue( value ); }
	bool ClearValue( const char *pszKey)																							{ return GetVM()->ClearValue( m_hScope, pszKey ); }

	ScriptStatus_t Run( HSCRIPT hScript )
	{
		InvalidateCachedValues();
		return GetVM()->Run( hScript, m_hScope );
	}

	ScriptStatus_t Run( const char *pszScriptText, const char *pszScriptName = nullptr )
	{
		InvalidateCachedValues();
		HSCRIPT hScript = GetVM()->CompileScript( pszScriptText, pszScriptName );
		if ( hScript )
		{
			ScriptStatus_t result = GetVM()->Run( hScript, m_hScope );
			GetVM()->ReleaseScript( hScript );
			return result; 
		}
		return SCRIPT_ERROR;
	}

	ScriptStatus_t Run( const unsigned char *pszScriptText, const char *pszScriptName = nullptr )
	{
		return Run( (const char *)pszScriptText, pszScriptName);
	}

	HSCRIPT LookupFunction( const char *pszFunction )
	{
		return GetVM()->LookupFunction( pszFunction, m_hScope );
	}

	void ReleaseFunction( HSCRIPT hScript )
	{
		GetVM()->ReleaseFunction( hScript );
	}

	bool FunctionExists( const char *pszFunction )
	{
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		GetVM()->ReleaseFunction( hFunction );
		return ( hFunction != NULL ) ;
	}

	//-----------------------------------------------------

	enum Flags_t
	{
		EXTERNAL = 0x01,
	};

	//-----------------------------------------------------

	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn = nullptr )
	{
		return GetVM()->ExecuteFunction( hFunction, NULL, 0, pReturn, m_hScope, true );
	}

	template <typename... TArgs>
	ScriptStatus_t Call( HSCRIPT hFunction, ScriptVariant_t *pReturn, TArgs... args )
	{
		constexpr std::size_t size = sizeof...(TArgs);

		ScriptVariant_t vars[size] = {std::forward(args)...};

		return GetVM()->ExecuteFunction( hFunction, vars, ARRAYSIZE(vars), pReturn, m_hScope, true );
	}

	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn = nullptr )
	{
		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, NULL, 0, pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}
	
	template <typename... TArgs>
	ScriptStatus_t Call( const char *pszFunction, ScriptVariant_t *pReturn, TArgs... args )
	{
		constexpr std::size_t size = sizeof...(TArgs);

		ScriptVariant_t vars[size] = {std::forward(args)...};

		HSCRIPT hFunction = GetVM()->LookupFunction( pszFunction, m_hScope );
		if ( !hFunction )
			return SCRIPT_ERROR;
		ScriptStatus_t status = GetVM()->ExecuteFunction( hFunction, vars, ARRAYSIZE(vars), pReturn, m_hScope, true );
		GetVM()->ReleaseFunction( hFunction );
		return status;
	}

protected:
	HSCRIPT m_hScope;
	int m_flags;
	CUtlVectorConservative<HSCRIPT *> m_FuncHandles;
};

using CScriptScope = CScriptScopeT<>;

#define VScriptAddEnumToScope_( scope, enumVal, scriptName )	(scope).SetValue( scriptName, (int)enumVal )
#define VScriptAddEnumToScope( scope, enumVal )					VScriptAddEnumToScope_( scope, enumVal, #enumVal )

#define VScriptAddEnumToRoot( enumVal )					g_pScriptVM->SetValue( #enumVal, (int)enumVal )

//-----------------------------------------------------------------------------
// Script function proxy support
//-----------------------------------------------------------------------------

class CScriptFuncHolder
{
public:
	CScriptFuncHolder() : hFunction( INVALID_HSCRIPT ) {}
	bool IsValid()	{ return ( hFunction != INVALID_HSCRIPT ); }
	bool IsNull()	{ return ( !hFunction ); }
	HSCRIPT hFunction;
};

#define DEFINE_SCRIPT_PROXY_GUTS( FuncName, N ) \
	CScriptFuncHolder m_hScriptFunc_##FuncName; \
	template < typename RET_TYPE FUNC_TEMPLATE_ARG_PARAMS_##N> \
	bool FuncName( RET_TYPE *pRetVal FUNC_ARG_FORMAL_PARAMS_##N ) \
	{ \
		if ( !m_hScriptFunc_##FuncName.IsValid() ) \
		{ \
			m_hScriptFunc_##FuncName.hFunction = LookupFunction( #FuncName ); \
			m_FuncHandles.AddToTail( &m_hScriptFunc_##FuncName.hFunction ); \
		} \
		\
		if ( !m_hScriptFunc_##FuncName.IsNull() ) \
		{ \
			ScriptVariant_t returnVal; \
			ScriptStatus_t result = Call( m_hScriptFunc_##FuncName.hFunction, &returnVal, FUNC_CALL_ARGS_##N ); \
			if ( result != SCRIPT_ERROR ) \
			{ \
				returnVal.AssignTo( pRetVal ); \
				returnVal.Free(); \
				return true; \
			} \
		} \
		return false; \
	}

#define DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, N ) \
	CScriptFuncHolder m_hScriptFunc_##FuncName; \
	template < FUNC_SOLO_TEMPLATE_ARG_PARAMS_##N> \
	bool FuncName( FUNC_PROXY_ARG_FORMAL_PARAMS_##N ) \
	{ \
		if ( !m_hScriptFunc_##FuncName.IsValid() ) \
		{ \
			m_hScriptFunc_##FuncName.hFunction = LookupFunction( #FuncName ); \
			m_FuncHandles.AddToTail( &m_hScriptFunc_##FuncName.hFunction ); \
		} \
		\
		if ( !m_hScriptFunc_##FuncName.IsNull() ) \
		{ \
			ScriptStatus_t result = Call( m_hScriptFunc_##FuncName.hFunction, NULL, FUNC_CALL_ARGS_##N ); \
			if ( result != SCRIPT_ERROR ) \
			{ \
				return true; \
			} \
		} \
		return false; \
	}

#define DEFINE_SCRIPT_PROXY_0V( FuncName ) \
	CScriptFuncHolder m_hScriptFunc_##FuncName; \
	bool FuncName() \
	{ \
		if ( !m_hScriptFunc_##FuncName.IsValid() ) \
		{ \
			m_hScriptFunc_##FuncName.hFunction = LookupFunction( #FuncName ); \
			m_FuncHandles.AddToTail( &m_hScriptFunc_##FuncName.hFunction ); \
		} \
		\
		if ( !m_hScriptFunc_##FuncName.IsNull() ) \
		{ \
			ScriptStatus_t result = Call( m_hScriptFunc_##FuncName.hFunction, NULL ); \
			if ( result != SCRIPT_ERROR ) \
			{ \
				return true; \
			} \
		} \
		return false; \
	}

#define DEFINE_SCRIPT_PROXY_0( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 0 )
#define DEFINE_SCRIPT_PROXY_1( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 1 )
#define DEFINE_SCRIPT_PROXY_2( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 2 )
#define DEFINE_SCRIPT_PROXY_3( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 3 )
#define DEFINE_SCRIPT_PROXY_4( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 4 )
#define DEFINE_SCRIPT_PROXY_5( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 5 )
#define DEFINE_SCRIPT_PROXY_6( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 6 )
#define DEFINE_SCRIPT_PROXY_7( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 7 )
#define DEFINE_SCRIPT_PROXY_8( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 8 )
#define DEFINE_SCRIPT_PROXY_9( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 9 )
#define DEFINE_SCRIPT_PROXY_10( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 10 )
#define DEFINE_SCRIPT_PROXY_11( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 11 )
#define DEFINE_SCRIPT_PROXY_12( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 12 )
#define DEFINE_SCRIPT_PROXY_13( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 13 )
#define DEFINE_SCRIPT_PROXY_14( FuncName ) DEFINE_SCRIPT_PROXY_GUTS( FuncName, 14 )

#define DEFINE_SCRIPT_PROXY_1V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 1 )
#define DEFINE_SCRIPT_PROXY_2V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 2 )
#define DEFINE_SCRIPT_PROXY_3V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 3 )
#define DEFINE_SCRIPT_PROXY_4V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 4 )
#define DEFINE_SCRIPT_PROXY_5V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 5 )
#define DEFINE_SCRIPT_PROXY_6V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 6 )
#define DEFINE_SCRIPT_PROXY_7V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 7 )
#define DEFINE_SCRIPT_PROXY_8V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 8 )
#define DEFINE_SCRIPT_PROXY_9V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 9 )
#define DEFINE_SCRIPT_PROXY_10V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 10 )
#define DEFINE_SCRIPT_PROXY_11V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 11 )
#define DEFINE_SCRIPT_PROXY_12V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 12 )
#define DEFINE_SCRIPT_PROXY_13V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 13 )
#define DEFINE_SCRIPT_PROXY_14V( FuncName ) DEFINE_SCRIPT_PROXY_GUTS_NO_RETVAL( FuncName, 14 )

//-----------------------------------------------------------------------------

template <>
inline HSCRIPT IScriptVM::Get<HSCRIPT>( HSCRIPT hScope, const char *pszKey )
{
	ScriptVariant_t variant;
	GetValue( hScope, pszKey, &variant );
	if ( variant.GetType() == FIELD_VOID )
		return nullptr;
	return variant.Get<HSCRIPT>();
}

#include "tier0/memdbgoff.h"

#endif // IVSCRIPT_H
