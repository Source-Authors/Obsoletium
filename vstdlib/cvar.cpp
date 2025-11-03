//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "vstdlib/cvar.h"
#include "tier0/icommandline.h"
#include "tier1/utlrbtree.h"
#include "tier1/strtools.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include "tier0/vprof.h"
#include "tier1/tier1.h"
#include "tier1/utlbuffer.h"

// dimhotepus: CS:GO backport.
#include "concommandhash.h"

#ifdef _X360
#include "xbox/xbox_console.h"
#endif

#ifdef POSIX
#include <wctype.h>
#include <wchar.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Default implementation  of CvarQuery
//-----------------------------------------------------------------------------
class CDefaultCvarQuery final : public CBaseAppSystem< ICvarQuery >
{
public:
	void *QueryInterface( const char *pInterfaceName ) override
	{
		if ( !Q_stricmp( pInterfaceName, CVAR_QUERY_INTERFACE_VERSION ) )
			return (ICvarQuery*)this;
		return nullptr;
	
	}

	bool AreConVarsLinkable( [[maybe_unused]] const ConVar *child, [[maybe_unused]] const ConVar *parent ) override
	{
		return true;
	}
};

static CDefaultCvarQuery s_DefaultCvarQuery;
static ICvarQuery *s_pCVarQuery = nullptr;


//-----------------------------------------------------------------------------
// Default implementation
//-----------------------------------------------------------------------------
class CCvar final : public ICvar
{
public:
	CCvar();

	// Methods of IAppSystem
	bool Connect( CreateInterfaceFn factory ) override;
	void Disconnect() override;
	void *QueryInterface( const char *pInterfaceName ) override;
	InitReturnVal_t Init() override;
	void Shutdown() override;

	// Inherited from ICVar
	CVarDLLIdentifier_t AllocateDLLIdentifier() override;
	void			RegisterConCommand( ConCommandBase *pCommandBase ) override;
	void			UnregisterConCommand( ConCommandBase *pCommandBase ) override;
	void			UnregisterConCommands( CVarDLLIdentifier_t id ) override;
	const char*		GetCommandLineValue( const char *pVariableName ) override;
	ConCommandBase *FindCommandBase( const char *name ) override;
	const ConCommandBase *FindCommandBase( const char *name ) const override;
	ConVar			*FindVar ( const char *var_name ) override;
	const ConVar	*FindVar ( const char *var_name ) const override;
	ConCommand		*FindCommand( const char *name ) override;
	const ConCommand *FindCommand( const char *name ) const override;
	ConCommandBase	*GetCommands( ) override;
	const ConCommandBase *GetCommands( ) const override;
	void			InstallGlobalChangeCallback( FnChangeCallback_t callback ) override;
	void			RemoveGlobalChangeCallback( FnChangeCallback_t callback ) override;
	void			CallGlobalChangeCallbacks( ConVar *var, const char *pOldString, float flOldValue ) override;
	void			InstallConsoleDisplayFunc( IConsoleDisplayFunc* pDisplayFunc ) override;
	void			RemoveConsoleDisplayFunc( IConsoleDisplayFunc* pDisplayFunc ) override;
	void			ConsoleColorPrintf( const Color& clr, PRINTF_FORMAT_STRING const char *pFormat, ... ) const override;
	void			ConsolePrintf( PRINTF_FORMAT_STRING const char *pFormat, ... ) const override;
	void			ConsoleDPrintf( PRINTF_FORMAT_STRING const char *pFormat, ... ) const override;
	void			RevertFlaggedConVars( int nFlag ) override;
	void			InstallCVarQuery( ICvarQuery *pQuery ) override;

	bool			IsMaterialThreadSetAllowed( ) const override;
	void			QueueMaterialThreadSetValue( ConVar *pConVar, const char *pValue ) override;
	void			QueueMaterialThreadSetValue( ConVar *pConVar, int nValue ) override;
	void			QueueMaterialThreadSetValue( ConVar *pConVar, float flValue ) override;
	bool			HasQueuedMaterialThreadConVarSets() const override;
	int				ProcessQueuedMaterialThreadConVarSets() override;
private:
	enum
	{
		CONSOLE_COLOR_PRINT = 0,
		CONSOLE_PRINT,
		CONSOLE_DPRINT,
	};

	void DisplayQueuedMessages( );

	CUtlVector< FnChangeCallback_t >	m_GlobalChangeCallbacks;
	CUtlVector< IConsoleDisplayFunc* >	m_DisplayFuncs;
	int									m_nNextDLLIdentifier;
	ConCommandBase						*m_pConCommandList;
	// dimhotepus: CS:GO backport for high-speed command search.
	CConCommandHash						m_CommandHash;

	// temporary console area so we can store prints before console display funs are installed
	mutable CUtlBuffer					m_TempConsoleBuffer;
protected:

	// internals for  ICVarIterator
	class CCVarIteratorInternal final : public ICVarIteratorInternal
	{
	public:
		CCVarIteratorInternal( CCvar *outer ) 
			: m_pOuter( outer )
			// dimhotepus: CS:GO backport.
			, m_pHash( &outer->m_CommandHash ), // remember my CCvar,
			m_hashIter( -1, -1 ) // and invalid iterator
			 
		{}
		void		SetFirst( ) RESTRICT override;
		void		Next( ) RESTRICT override;
		bool		IsValid( ) RESTRICT override;
		RESTRICT_FUNC ConCommandBase *Get( ) override;
	protected:
		CCvar * const m_pOuter;
		// dimhotepus: CS:GO backport.
		CConCommandHash * const m_pHash;
		CConCommandHash::CCommandHashIterator_t m_hashIter;
		// ConCommandBase *m_pCur{ nullptr };
	};

	ICVarIteratorInternal	*FactoryInternalIterator( ) override;
	friend class CCVarIteratorInternal;

	enum ConVarSetType_t
	{
		CONVAR_SET_STRING = 0,
		CONVAR_SET_INT,
		CONVAR_SET_FLOAT,
	};
	struct QueuedConVarSet_t
	{
		ConVar *m_pConVar;
		ConVarSetType_t m_nType;
		int m_nInt;
		float m_flFloat;
		CUtlString m_String;
	};
	CUtlVector< QueuedConVarSet_t > m_QueuedConVarSets;
	bool m_bMaterialSystemThreadSetAllowed;

private:
	// Standard console commands -- DO NOT PLACE ANY HIGHER THAN HERE BECAUSE THESE MUST BE THE FIRST TO DESTRUCT
	CON_COMMAND_MEMBER_F( CCvar, "find", Find, "Find concommands with the specified string in their name/help text.", 0 )
	// dimhotepus: CS:GO backport.
#ifdef _DEBUG
	CON_COMMAND_MEMBER_F( CCvar, "ccvar_hash_report", HashReport, "report info on bucket distribution of internal hash.", 0 )
#endif
};

void CCvar::CCVarIteratorInternal::SetFirst( ) RESTRICT
{
	// dimhotepus: CS:GO backport.
	m_hashIter = m_pHash->First();
	//m_pCur = m_pOuter->GetCommands();
}

void CCvar::CCVarIteratorInternal::Next( ) RESTRICT
{
	// dimhotepus: CS:GO backport.
	m_hashIter = m_pHash->Next( m_hashIter );
	//if ( m_pCur )
	//	m_pCur = m_pCur->GetNext();
}

bool CCvar::CCVarIteratorInternal::IsValid( ) RESTRICT
{
	// dimhotepus: CS:GO backport.
	return m_pHash->IsValidIterator( m_hashIter );
	// return m_pCur != nullptr;
}

RESTRICT_FUNC ConCommandBase *CCvar::CCVarIteratorInternal::Get( )
{
	Assert( IsValid( ) );
	// dimhotepus: CS:GO backport.
	return (*m_pHash)[m_hashIter];
	// return m_pCur;
}

ICvar::ICVarIteratorInternal *CCvar::FactoryInternalIterator( )
{
	return new CCVarIteratorInternal( this );
}

//-----------------------------------------------------------------------------
// Factor for CVars 
//-----------------------------------------------------------------------------
static CCvar s_Cvar;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CCvar, ICvar, CVAR_INTERFACE_VERSION, s_Cvar );


//-----------------------------------------------------------------------------
// Returns a CVar dictionary for tool usage
//-----------------------------------------------------------------------------
CreateInterfaceFn VStdLib_GetICVarFactory()
{
	return Sys_GetFactoryThis();
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CCvar::CCvar() : m_TempConsoleBuffer( (intp)0, 1024 )
{
	// dimhotepus: Preallocate diplay funcs.
	m_DisplayFuncs.EnsureCapacity( 2 );

	m_nNextDLLIdentifier = 0;
	m_pConCommandList = nullptr;
	// dimhotepus: CS:GO backport.
	m_CommandHash.Init();

	m_bMaterialSystemThreadSetAllowed = false;
}


//-----------------------------------------------------------------------------
// Methods of IAppSystem
//-----------------------------------------------------------------------------
bool CCvar::Connect( CreateInterfaceFn factory )
{
	ConnectTier1Libraries( &factory, 1 );

	s_pCVarQuery = (ICvarQuery*)factory( CVAR_QUERY_INTERFACE_VERSION, nullptr );
	if ( !s_pCVarQuery )
	{
		s_pCVarQuery = &s_DefaultCvarQuery;
	}

	ConVar_Register();
	return true;
}

void CCvar::Disconnect()
{
	ConVar_Unregister();
	s_pCVarQuery = nullptr;
	DisconnectTier1Libraries();
}

InitReturnVal_t CCvar::Init()
{
	return INIT_OK;
}

void CCvar::Shutdown()
{
}

void *CCvar::QueryInterface( const char *pInterfaceName )
{
	// We implement the ICvar interface
	if ( !V_strcmp( pInterfaceName, CVAR_INTERFACE_VERSION ) )
		return (ICvar*)this;

	return nullptr;
}


//-----------------------------------------------------------------------------
// Method allowing the engine ICvarQuery interface to take over
//-----------------------------------------------------------------------------
void CCvar::InstallCVarQuery( ICvarQuery *pQuery )
{
	Assert( s_pCVarQuery == &s_DefaultCvarQuery );
	s_pCVarQuery = pQuery ? pQuery : &s_DefaultCvarQuery;
}


//-----------------------------------------------------------------------------
// Used by DLLs to be able to unregister all their commands + convars 
//-----------------------------------------------------------------------------
CVarDLLIdentifier_t CCvar::AllocateDLLIdentifier()
{
	return m_nNextDLLIdentifier++;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *variable - 
//-----------------------------------------------------------------------------
void CCvar::RegisterConCommand( ConCommandBase *variable )
{
	// Already registered
	if ( variable->IsRegistered() )
		return;

	variable->m_bRegistered = true;

	const char *pName = variable->GetName();
	if ( !pName || !pName[0] )
	{
		variable->m_pNext = nullptr;
		return;
	}

	// If the variable is already defined, then setup the new variable as a proxy to it.
	const ConCommandBase *pOther = FindVar( variable->GetName() );
	if ( pOther )
	{
		if ( variable->IsCommand() || pOther->IsCommand() )
		{
			Warning( "WARNING: unable to link %s and %s because one or more is a ConCommand.\n", variable->GetName(), pOther->GetName() );
		}
		else
		{
			// This cast is ok because we make sure they're ConVars above.
			const auto *pChildVar = static_cast< const ConVar* >( variable );
			const auto *pParentVar = static_cast< const ConVar* >( pOther );

			// See if it's a valid linkage
			if ( s_pCVarQuery->AreConVarsLinkable( pChildVar, pParentVar ) )
			{
				// Make sure the default values are the same (but only spew about this for FCVAR_REPLICATED)
				if(  pChildVar->m_pszDefaultValue && pParentVar->m_pszDefaultValue &&
					 pChildVar->IsFlagSet( FCVAR_REPLICATED ) && pParentVar->IsFlagSet( FCVAR_REPLICATED ) )
				{
					if( Q_stricmp( pChildVar->m_pszDefaultValue, pParentVar->m_pszDefaultValue ) != 0 )
					{
						Warning( "Parent and child ConVars with different default values! %s child: %s parent: %s (parent wins)\n", 
							variable->GetName(), pChildVar->m_pszDefaultValue, pParentVar->m_pszDefaultValue );
					}
				}

				const_cast<ConVar*>( pChildVar )->m_pParent = const_cast<ConVar*>( pParentVar )->m_pParent;

				// Absorb material thread related convar flags
				const_cast<ConVar*>( pParentVar )->m_nFlags |= pChildVar->m_nFlags & ( FCVAR_MATERIAL_THREAD_MASK | FCVAR_ACCESSIBLE_FROM_THREADS );

				// check the parent's callbacks and slam if doesn't have, warn if both have callbacks
				if(  pChildVar->m_fnChangeCallback )
				{
					if ( !pParentVar->m_fnChangeCallback )
					{
						const_cast<ConVar*>( pParentVar )->m_fnChangeCallback = pChildVar->m_fnChangeCallback;
					}
					// dimhotepus: Warn only if convars have different callbacks.
					else if ( pParentVar->m_fnChangeCallback != pChildVar->m_fnChangeCallback )
					{
						Warning( "Convar %s has multiple different change callbacks\n", variable->GetName() );
					}
				}

				// make sure we don't have conflicting help strings.
				if ( pChildVar->m_pszHelpString && !Q_isempty( pChildVar->m_pszHelpString ) )
				{
					if ( pParentVar->m_pszHelpString && !Q_isempty( pParentVar->m_pszHelpString ) )
					{
						if ( Q_stricmp( pParentVar->m_pszHelpString, pChildVar->m_pszHelpString ) != 0 )
						{
							Warning( "Convar %s has multiple help strings:\n\tparent (wins): \"%s\"\n\tchild: \"%s\"\n", 
								variable->GetName(), pParentVar->m_pszHelpString, pChildVar->m_pszHelpString );
						}
					}
					else
					{
						const_cast<ConVar *>( pParentVar )->m_pszHelpString = pChildVar->m_pszHelpString;
					}
				}

				// make sure we don't have conflicting FCVAR_CHEAT flags.
				if ( ( pChildVar->m_nFlags & FCVAR_CHEAT ) != ( pParentVar->m_nFlags & FCVAR_CHEAT ) )
				{
					Warning( "Convar %s has conflicting FCVAR_CHEAT flags (child: %s, parent: %s, parent wins)\n", 
						variable->GetName(), ( pChildVar->m_nFlags & FCVAR_CHEAT ) ? "FCVAR_CHEAT" : "no FCVAR_CHEAT",
						( pParentVar->m_nFlags & FCVAR_CHEAT ) ? "FCVAR_CHEAT" : "no FCVAR_CHEAT" );
				}

				// make sure we don't have conflicting FCVAR_REPLICATED flags.
				if ( ( pChildVar->m_nFlags & FCVAR_REPLICATED ) != ( pParentVar->m_nFlags & FCVAR_REPLICATED ) )
				{
					Warning( "Convar %s has conflicting FCVAR_REPLICATED flags (child: %s, parent: %s, parent wins)\n", 
						variable->GetName(), ( pChildVar->m_nFlags & FCVAR_REPLICATED ) ? "FCVAR_REPLICATED" : "no FCVAR_REPLICATED",
						( pParentVar->m_nFlags & FCVAR_REPLICATED ) ? "FCVAR_REPLICATED" : "no FCVAR_REPLICATED" );
				}

				// make sure we don't have conflicting FCVAR_DONTRECORD flags.
				if ( ( pChildVar->m_nFlags & FCVAR_DONTRECORD ) != ( pParentVar->m_nFlags & FCVAR_DONTRECORD ) )
				{
					Warning( "Convar %s has conflicting FCVAR_DONTRECORD flags (child: %s, parent: %s, parent wins)\n", 
						variable->GetName(), ( pChildVar->m_nFlags & FCVAR_DONTRECORD ) ? "FCVAR_DONTRECORD" : "no FCVAR_DONTRECORD",
						( pParentVar->m_nFlags & FCVAR_DONTRECORD ) ? "FCVAR_DONTRECORD" : "no FCVAR_DONTRECORD" );
				}
			}
		}

		variable->m_pNext = nullptr;
		return;
	}

	// link the variable in
	variable->m_pNext = m_pConCommandList;
	m_pConCommandList = variable;
	
	// dimhotepus: CS:GO backport.
	AssertMsg1(FindCommandBase(variable->GetName()) == nullptr, "Console command %s added twice!",
		variable->GetName());
	m_CommandHash.Insert(variable);
}

void CCvar::UnregisterConCommand( ConCommandBase *pCommandToRemove )
{
	// Not registered? Don't bother
	if ( !pCommandToRemove->IsRegistered() )
		return;

	pCommandToRemove->m_bRegistered = false;

	// FIXME: Should we make this a doubly-linked list? Would remove faster
	ConCommandBase *pPrev = nullptr;
	for( ConCommandBase *pCommand = m_pConCommandList; pCommand; pCommand = pCommand->m_pNext )
	{
		if ( pCommand != pCommandToRemove )
		{
			pPrev = pCommand;
			continue;
		}

		if ( pPrev == nullptr )
		{
			m_pConCommandList = pCommand->m_pNext;
		}
		else
		{
			pPrev->m_pNext = pCommand->m_pNext;
		}
		pCommand->m_pNext = nullptr;
		// dimhotepus: CS:GO backport.
		m_CommandHash.Remove(m_CommandHash.Find(pCommand));
		break;
	}
}

void CCvar::UnregisterConCommands( CVarDLLIdentifier_t id )
{
	ConCommandBase *pNewList = nullptr;
	ConCommandBase  *pCommand = m_pConCommandList;
	// dimhotepus: CS:GO backport.
	m_CommandHash.Purge( true );
	while ( pCommand )
	{
		ConCommandBase  *pNext = pCommand->m_pNext;
		if ( pCommand->GetDLLIdentifier() != id )
		{
			pCommand->m_pNext = pNewList;
			pNewList = pCommand;
			// dimhotepus: CS:GO backport.
			m_CommandHash.Insert( pCommand );
		}
		else
		{
			// Unlink
			pCommand->m_bRegistered = false;
			pCommand->m_pNext = nullptr;
		}

		pCommand = pNext;
	}

	m_pConCommandList = pNewList;
}


//-----------------------------------------------------------------------------
// Finds base commands 
//-----------------------------------------------------------------------------
const ConCommandBase *CCvar::FindCommandBase( const char *name ) const
{
	// dimhotepus: CS:GO backport.
	VPROF_INCREMENT_COUNTER( "CCvar::FindCommandBase", 1 );
	VPROF_BUDGET( "CCvar::FindCommandBase", VPROF_BUDGETGROUP_CVAR_FIND );
	
	return m_CommandHash.FindPtr( name );
}

ConCommandBase *CCvar::FindCommandBase( const char *name )
{
	// dimhotepus: CS:GO backport.
	VPROF_INCREMENT_COUNTER( "CCvar::FindCommandBase", 1 );
	VPROF_BUDGET( "CCvar::FindCommandBase", VPROF_BUDGETGROUP_CVAR_FIND );
	
	return m_CommandHash.FindPtr( name );
}


//-----------------------------------------------------------------------------
// Purpose Finds ConVars
//-----------------------------------------------------------------------------
const ConVar *CCvar::FindVar( const char *var_name ) const
{
	VPROF_INCREMENT_COUNTER( "CCvar::FindVar", 1 );
	VPROF( "CCvar::FindVar" );
	const ConCommandBase *var = FindCommandBase( var_name );
	if ( !var || var->IsCommand() )
		return nullptr;
	
	return static_cast<const ConVar*>(var);
}

ConVar *CCvar::FindVar( const char *var_name )
{
	VPROF_INCREMENT_COUNTER( "CCvar::FindVar", 1 );
	VPROF( "CCvar::FindVar" );
	ConCommandBase *var = FindCommandBase( var_name );
	if ( !var || var->IsCommand() )
		return nullptr;
	
	return static_cast<ConVar*>( var );
}


//-----------------------------------------------------------------------------
// Purpose Finds ConCommands
//-----------------------------------------------------------------------------
const ConCommand *CCvar::FindCommand( const char *pCommandName ) const
{
	const ConCommandBase *var = FindCommandBase( pCommandName );
	if ( !var || !var->IsCommand() )
		return nullptr;

	return static_cast<const ConCommand*>(var);
}

ConCommand *CCvar::FindCommand( const char *pCommandName )
{
	ConCommandBase *var = FindCommandBase( pCommandName );
	if ( !var || !var->IsCommand() )
		return nullptr;

	return static_cast<ConCommand*>( var );
}


const char* CCvar::GetCommandLineValue( const char *pVariableName )
{
	intp nLen = Q_strlen(pVariableName);
	char *pSearch = (char*)stackalloc( nLen + 2 );
	pSearch[0] = '+';
	memcpy( &pSearch[1], pVariableName, nLen + 1 );
	return CommandLine()->ParmValue( pSearch );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ConCommandBase *CCvar::GetCommands( )
{
	return m_pConCommandList;
}

const ConCommandBase *CCvar::GetCommands( ) const
{
	return m_pConCommandList;
}


//-----------------------------------------------------------------------------
// Install, remove global callbacks
//-----------------------------------------------------------------------------
void CCvar::InstallGlobalChangeCallback( FnChangeCallback_t callback )
{
	Assert( callback && m_GlobalChangeCallbacks.Find( callback ) < 0 );
	m_GlobalChangeCallbacks.AddToTail( callback );
}

void CCvar::RemoveGlobalChangeCallback( FnChangeCallback_t callback )
{
	Assert( callback );
	m_GlobalChangeCallbacks.FindAndRemove( callback );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCvar::CallGlobalChangeCallbacks( ConVar *var, const char *pOldString, float flOldValue )
{
	for ( auto c : m_GlobalChangeCallbacks )
	{
		c( var, pOldString, flOldValue );
	}
}


//-----------------------------------------------------------------------------
// Sets convars containing the flags to their default value
//-----------------------------------------------------------------------------
void CCvar::RevertFlaggedConVars( int nFlag )
{
	// dimhotepus: CS:GO backport.
	for ( auto i = m_CommandHash.First() ;
		  m_CommandHash.IsValidIterator( i ) ; 
		  i = m_CommandHash.Next( i ) )
	{
		ConCommandBase *var = m_CommandHash[ i ];
		if ( var->IsCommand() )
			continue;

		auto *pCvar = ( ConVar * )var;

		if ( !pCvar->IsFlagSet( nFlag ) )
			continue;

		// It's == to the default value, don't count
		if ( !Q_stricmp( pCvar->GetDefault(), pCvar->GetString() ) )
			continue;

		pCvar->Revert();

		// DevMsg( "%s = \"%s\" (reverted)\n", cvar->GetName(), cvar->GetString() );
	}
}


//-----------------------------------------------------------------------------
// Deal with queued material system convars
//-----------------------------------------------------------------------------
bool CCvar::IsMaterialThreadSetAllowed( ) const
{
	Assert( ThreadInMainThread() );
	return m_bMaterialSystemThreadSetAllowed;
}

void CCvar::QueueMaterialThreadSetValue( ConVar *pConVar, const char *pValue )
{
	Assert( ThreadInMainThread() );
	intp j = m_QueuedConVarSets.AddToTail();
	m_QueuedConVarSets[j].m_pConVar = pConVar;
	m_QueuedConVarSets[j].m_nType = CONVAR_SET_STRING;
	m_QueuedConVarSets[j].m_String = pValue;
}

void CCvar::QueueMaterialThreadSetValue( ConVar *pConVar, int nValue )
{
	Assert( ThreadInMainThread() );
	intp j = m_QueuedConVarSets.AddToTail();
	m_QueuedConVarSets[j].m_pConVar = pConVar;
	m_QueuedConVarSets[j].m_nType = CONVAR_SET_INT;
	m_QueuedConVarSets[j].m_nInt = nValue;
}

void CCvar::QueueMaterialThreadSetValue( ConVar *pConVar, float flValue )
{
	Assert( ThreadInMainThread() );
	intp j = m_QueuedConVarSets.AddToTail();
	m_QueuedConVarSets[j].m_pConVar = pConVar;
	m_QueuedConVarSets[j].m_nType = CONVAR_SET_FLOAT;
	m_QueuedConVarSets[j].m_flFloat = flValue;
}

bool CCvar::HasQueuedMaterialThreadConVarSets() const
{
	Assert( ThreadInMainThread() );
	return m_QueuedConVarSets.Count() > 0;
}

int CCvar::ProcessQueuedMaterialThreadConVarSets()
{
	Assert( ThreadInMainThread() );
	m_bMaterialSystemThreadSetAllowed = true;

	int nUpdateFlags = 0;
	for ( const auto& set : m_QueuedConVarSets )
	{
		switch( set.m_nType )
		{
		case CONVAR_SET_FLOAT:
			set.m_pConVar->SetValue( set.m_flFloat );
			break;
		case CONVAR_SET_INT:
			set.m_pConVar->SetValue( set.m_nInt );
			break;
		case CONVAR_SET_STRING:
			set.m_pConVar->SetValue( set.m_String );
			break;
		}

		nUpdateFlags |= set.m_pConVar->GetFlags() & FCVAR_MATERIAL_THREAD_MASK;
	}

	m_QueuedConVarSets.RemoveAll();
	m_bMaterialSystemThreadSetAllowed = false;
	return nUpdateFlags;
}


//-----------------------------------------------------------------------------
// Display queued messages
//-----------------------------------------------------------------------------
void CCvar::DisplayQueuedMessages( )
{
	// Display any queued up messages
	if ( m_TempConsoleBuffer.TellPut() == 0 )
		return;

	Color clr;
	intp nStringLength;
	while( m_TempConsoleBuffer.IsValid() )
	{
		int nType = m_TempConsoleBuffer.GetChar();
		if ( nType == CONSOLE_COLOR_PRINT )
		{
			clr.SetRawColor( m_TempConsoleBuffer.GetInt() );
		}
		nStringLength = m_TempConsoleBuffer.PeekStringLength();
		char* pTemp = stackallocT( char, nStringLength + 1 );
		m_TempConsoleBuffer.GetStringManualCharCount( pTemp, nStringLength + 1 );

		switch( nType )
		{
		case CONSOLE_COLOR_PRINT:
			ConsoleColorPrintf( clr, pTemp );
			break;

		case CONSOLE_PRINT:
			ConsolePrintf( pTemp );
			break;

		case CONSOLE_DPRINT:
			ConsoleDPrintf( pTemp );
			break;
		}
	}

	m_TempConsoleBuffer.Purge();
}


//-----------------------------------------------------------------------------
// Install a console printer
//-----------------------------------------------------------------------------
void CCvar::InstallConsoleDisplayFunc( IConsoleDisplayFunc* pDisplayFunc )
{
	Assert( m_DisplayFuncs.Find( pDisplayFunc ) < 0 );
	m_DisplayFuncs.AddToTail( pDisplayFunc );
	DisplayQueuedMessages();
}

void CCvar::RemoveConsoleDisplayFunc( IConsoleDisplayFunc* pDisplayFunc )
{
	m_DisplayFuncs.FindAndRemove( pDisplayFunc );
}

void CCvar::ConsoleColorPrintf( const Color& clr, PRINTF_FORMAT_STRING const char *pFormat, ... ) const
{
	char temp[ 8192 ];
	va_list argptr;
	va_start( argptr, pFormat );
	V_vsprintf_safe( temp, pFormat, argptr );
	va_end( argptr );

	intp c = m_DisplayFuncs.Count();
	if ( c == 0 )
	{
		m_TempConsoleBuffer.PutChar( CONSOLE_COLOR_PRINT );
		m_TempConsoleBuffer.PutInt( clr.GetRawColor() );
		m_TempConsoleBuffer.PutString( temp );
		return;
	}

	for ( auto f : m_DisplayFuncs )
	{
		f->ColorPrint( clr, temp );
	}
}

void CCvar::ConsolePrintf( PRINTF_FORMAT_STRING const char *pFormat, ... ) const
{
	char temp[ 8192 ];
	va_list argptr;
	va_start( argptr, pFormat );
	V_sprintf_safe( temp, pFormat, argptr );
	va_end( argptr );

	intp c = m_DisplayFuncs.Count();
	if ( c == 0 )
	{
		m_TempConsoleBuffer.PutChar( CONSOLE_PRINT );
		m_TempConsoleBuffer.PutString( temp );
		return;
	}

	for ( intp i = 0 ; i < c; ++i )
	{
		m_DisplayFuncs[ i ]->Print( temp );
	}
}

void CCvar::ConsoleDPrintf( PRINTF_FORMAT_STRING const char *pFormat, ... ) const
{
	char temp[ 8192 ];
	va_list argptr;
	va_start( argptr, pFormat );
	V_vsprintf_safe( temp, pFormat, argptr );
	va_end( argptr );

	intp c = m_DisplayFuncs.Count();
	if ( c == 0 )
	{
		m_TempConsoleBuffer.PutChar( CONSOLE_DPRINT );
		m_TempConsoleBuffer.PutString( temp );
		return;
	}

	for ( auto f : m_DisplayFuncs )
	{
		f->DPrint( temp );
	}
}

// dimhotepus: CS:GO backport.
static bool ConVarSortFunc( ConCommandBase * const &lhs, ConCommandBase * const &rhs )
{
	return CaselessStringLessThan( lhs->GetName(), rhs->GetName() );
}

//-----------------------------------------------------------------------------
// Console commands
//-----------------------------------------------------------------------------
void CCvar::Find( const CCommand &args )
{
	const char *search;

	if ( args.ArgC() != 2 )
	{
		ConMsg( "Usage:  find <string>\n" );
		return;
	}

	// Get substring to find
	search = args[1];

	// dimhotepus: CS:GO backport. Sort commands.
	CUtlRBTree< ConCommandBase *, int > sorted( 0, 0, ConVarSortFunc );

	// dimhotepus: CS:GO backport.
	// Loop through vars and print out findings
	for ( auto i = m_CommandHash.First() ;
		m_CommandHash.IsValidIterator(i) ; 
		i = m_CommandHash.Next(i) )
	{
		ConCommandBase *var = m_CommandHash[ i ];
		if ( var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN) )
			continue;

		if ( !V_stristr( var->GetName(), search ) &&
			!V_stristr( var->GetHelpText(), search ) )
			continue;

		sorted.Insert( var );
	}

	// dimhotepus: CS:GO backport. Print sorted commands.
	for ( auto i = sorted.FirstInorder(); i != sorted.InvalidIndex(); i = sorted.NextInorder( i ) )
	{
		ConVar_PrintDescription( sorted[ i ] );
	}
}

#ifdef _DEBUG
void CCvar::HashReport( const CCommand &args )
{
	m_CommandHash.Report();
}
#endif


// dimhotepus: CS:GO backport.
//-----------------------------------------------------------------------------
// Console command hash data structure
//-----------------------------------------------------------------------------
CConCommandHash::CConCommandHash()
{
	Purge( true );
}

CConCommandHash::~CConCommandHash()
{
	Purge( false );
}

void CConCommandHash::Purge( bool bReinitialize )
{
	m_aBuckets.Purge();
	m_aDataPool.Purge();
	if ( bReinitialize )
	{
		Init();
	}
}

// Initialize.
void CConCommandHash::Init()
{
	// kNUM_BUCKETS must be a power of two.
	static_assert( IsPowerOfTwo( to_underlying( kNUM_BUCKETS ) ) );

	// Set the bucket size.
	m_aBuckets.SetSize( kNUM_BUCKETS );
	for ( intp iBucket = 0; iBucket < to_underlying( kNUM_BUCKETS ); ++iBucket )
	{
		m_aBuckets[iBucket] = m_aDataPool.InvalidIndex();
	}

	// Calculate the grow size.
	constexpr intp nGrowSize = 4 * to_underlying( kNUM_BUCKETS );
	m_aDataPool.SetGrowSize( nGrowSize );
}

//-----------------------------------------------------------------------------
// Purpose: Insert data into the hash table given its key (unsigned int), 
//			WITH a check to see if the element already exists within the hash.
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashHandle_t CConCommandHash::Insert( ConCommandBase *cmd )
{
	// Check to see if that key already exists in the buckets (should be unique).
	CCommandHashHandle_t hHash = Find( cmd );
	if( hHash != InvalidHandle() )
		return hHash;

	return FastInsert( cmd );
}
//-----------------------------------------------------------------------------
// Purpose: Insert data into the hash table given its key (unsigned int),
//          WITHOUT a check to see if the element already exists within the hash.
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashHandle_t CConCommandHash::FastInsert( ConCommandBase *cmd )
{
	// Get a new element from the pool.
	intp iHashData = m_aDataPool.Alloc( true );
	HashEntry_t * RESTRICT pHashData = &m_aDataPool[iHashData];
	if ( !pHashData )
		return InvalidHandle();

	HashKey_t key = Hash(cmd);

	// Add data to new element.
	pHashData->m_uiKey = key;
	pHashData->m_Data = cmd;

	// Link element.
	intp iBucket = key & to_underlying( kBUCKETMASK ); // HashFuncs::Hash( uiKey, m_uiBucketMask );
	m_aDataPool.LinkBefore( m_aBuckets[iBucket], iHashData );
	m_aBuckets[iBucket] = iHashData;

	return iHashData;
}

//-----------------------------------------------------------------------------
// Purpose: Remove a given element from the hash.
//-----------------------------------------------------------------------------
void CConCommandHash::Remove( CCommandHashHandle_t hHash ) RESTRICT
{
	HashEntry_t * RESTRICT entry = &m_aDataPool[hHash];
	HashKey_t iBucket = entry->m_uiKey & to_underlying( kBUCKETMASK );
	if ( m_aBuckets[iBucket] == hHash )
	{
		// It is a bucket head.
		m_aBuckets[iBucket] = m_aDataPool.Next( hHash );
	}
	else
	{
		// Not a bucket head.
		m_aDataPool.Unlink( hHash );
	}

	// Remove the element.
	m_aDataPool.Remove( hHash );
}

//-----------------------------------------------------------------------------
// Purpose: Remove all elements from the hash
//-----------------------------------------------------------------------------
void CConCommandHash::RemoveAll()
{
	m_aBuckets.RemoveAll();
	m_aDataPool.RemoveAll();
}

//-----------------------------------------------------------------------------
// Find hash entry corresponding to a string name
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashHandle_t CConCommandHash::Find( const char *name, HashKey_t hashkey) const RESTRICT
{
	// hash the "key" - get the correct hash table "bucket"
	intp iBucket = hashkey & to_underlying( kBUCKETMASK );

	for ( auto iElement = m_aBuckets[iBucket]; iElement != m_aDataPool.InvalidIndex(); iElement = m_aDataPool.Next( iElement ) )
	{
		const HashEntry_t &element = m_aDataPool[iElement];
		if ( element.m_uiKey == hashkey && // if hashes of strings match,
			 Q_stricmp( name, element.m_Data->GetName() ) == 0) // then test the actual strings
		{
			return iElement;
		}
	}

	// found nuffink
	return InvalidHandle();
}

//-----------------------------------------------------------------------------
// Find a command in the hash.
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashHandle_t CConCommandHash::Find( const ConCommandBase *cmd ) const RESTRICT
{
	// Set this #if to 1 if the assert at bottom starts whining --
	// that indicates that a console command is being double-registered,
	// or something similarly nonfatally bad. With this #if 1, we'll search
	// by name instead of by pointer, which is more robust in the face
	// of double registered commands, but obviously slower.
#if 0 
	return Find(cmd->GetName());
#else
	HashKey_t hashkey = Hash(cmd);
	intp iBucket = hashkey & to_underlying( kBUCKETMASK );

	// hunt through all entries in that bucket
	for ( auto iElement = m_aBuckets[iBucket]; iElement != m_aDataPool.InvalidIndex(); iElement = m_aDataPool.Next( iElement ) )
	{
		const HashEntry_t &element = m_aDataPool[iElement];
		if ( element.m_uiKey == hashkey && // if the hashes match... 
			 element.m_Data  == cmd	) // and the pointers...
		{
			// in debug, test to make sure we don't have commands under the same name
			// or something goofy like that
			AssertMsg1( iElement == Find(cmd->GetName()),
				"ConCommand %s had two entries in the hash!", cmd->GetName() );
			
			// return this element
			return iElement;
		}
	}

	// found nothing.
#ifdef DBGFLAG_ASSERT // double check against search by name
	CCommandHashHandle_t dbghand = Find(cmd->GetName());

	AssertMsg1( InvalidHandle() == dbghand,
		"ConCommand %s couldn't be found by pointer, but was found by name!", cmd->GetName() );
#endif
	return InvalidHandle();
#endif
}


#ifdef _DEBUG
// Dump a report to MSG
void CConCommandHash::Report()
{
	Msg("Console command hash bucket load:\n");
	intp total = 0;
	for ( intp iBucket = 0 ; iBucket < to_underlying( kNUM_BUCKETS); ++iBucket )
	{
		intp count = 0;
		CCommandHashHandle_t iElement = m_aBuckets[iBucket]; // get the head of the bucket
		while ( iElement != m_aDataPool.InvalidIndex() )
		{
			++count;
			iElement = m_aDataPool.Next( iElement );
		}

		Msg( "%zd: %zd\n", iBucket, count );
		total += count;
	}

	Msg("\tAverage: %.1f\n", total / static_cast<float>(to_underlying(kNUM_BUCKETS)));
}
#endif

