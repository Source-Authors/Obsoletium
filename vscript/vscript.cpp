//========== Copyright © 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "vscript/ivscript.h"

#include "languages/squirrel/vsquirrel/vsquirrel.h"
#include "languages/lua/vlua/vlua.h" 

#include "tier1/interface.h"
#include "tier1/tier1.h"

namespace {

class CScriptManager final : public CTier1AppSystem<IScriptManager>
{
public:
	IScriptVM *CreateVM( ScriptLanguage_t language ) override
	{
		IScriptVM *pVM = nullptr;

		if ( language == SL_SQUIRREL )
		{
			pVM = ScriptCreateSquirrelVM();
		}
		else if ( language == SL_LUA )
		{
			pVM = ScriptCreateLuaVM();
		}
		else
		{
			DWarning( "vscript", 0, "Unsupported script language 0x%x.\n", language );
			AssertMsg( 0, "Unsupported script language %d.", language );
			return nullptr;
		}

		if ( pVM )
		{
			// dimhotepus: Handle init failure.
			if ( !pVM->Init() )
			{
				DestroyVM( pVM );
				return nullptr;
			}

			ScriptRegisterFunction( pVM, RandomFloat, "Generate a random floating point number within a range, inclusive" );
			ScriptRegisterFunction( pVM, RandomInt, "Generate a random integer within a range, inclusive" );
		}
		return pVM;
	}

	void DestroyVM( IScriptVM *p ) override
	{
		if ( p )
		{
			p->Shutdown();

			ScriptLanguage_t language = p->GetLanguage();

			if ( language == SL_SQUIRREL )
			{
				ScriptDestroySquirrelVM( p );
			}
			else if ( language == SL_LUA )
			{
				ScriptDestroyLuaVM( p );
			}
			else
			{
				DWarning( "vscript", 0, "Unsupported script language 0x%x.\n", language );
				AssertMsg( 0, "Unsupported script language 0x%x.", language );
			}
		}
	}
};

// Singleton
CScriptManager g_ScriptManager;

}  // namespace

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CScriptManager, IScriptManager, VSCRIPT_INTERFACE_VERSION, g_ScriptManager );
