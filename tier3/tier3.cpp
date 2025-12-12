//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//

#include "tier3/tier3.h"
#include "tier0/dbg.h"
#include "istudiorender.h"
#include "vgui/IVGui.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/ISystem.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "video/ivideoservices.h"
#include "movieobjects/idmemakefileutils.h"
#include "vphysics_interface.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "ivtex.h"


//-----------------------------------------------------------------------------
// These tier3 libraries must be set by any users of this library.
// They can be set by calling ConnectTier3Libraries.
// It is hoped that setting this, and using this library will be the common mechanism for
// allowing link libraries to access tier3 library interfaces
//-----------------------------------------------------------------------------
IStudioRender *g_pStudioRender = nullptr;
IStudioRender *studiorender = nullptr;
IMatSystemSurface *g_pMatSystemSurface = nullptr;
vgui::IInput *g_pVGuiInput = nullptr;
vgui::ISurface *g_pVGuiSurface = nullptr;
vgui::IPanel *g_pVGuiPanel = nullptr;
vgui::IVGui	*g_pVGui = nullptr;
vgui::ILocalize *g_pVGuiLocalize = nullptr;
vgui::ISchemeManager *g_pVGuiSchemeManager = nullptr;
vgui::ISystem *g_pVGuiSystem = nullptr;
IDataCache *g_pDataCache = nullptr;
IMDLCache *g_pMDLCache = nullptr;
IMDLCache *mdlcache = nullptr;
IVideoServices *g_pVideo = nullptr;
IDmeMakefileUtils *g_pDmeMakefileUtils = nullptr;
IPhysicsCollision *g_pPhysicsCollision = nullptr;
ISoundEmitterSystemBase *g_pSoundEmitterSystem = nullptr;
IVTex *g_pVTex = nullptr;

namespace
{

template<typename T>
T* ConnectLibrary( T **library,
	CreateInterfaceFn *factory_list, intp index,
	const char *interface_name )
{
	if ( !*library )
	{
		return (*library = static_cast<T*>(factory_list[index]( interface_name, nullptr )));
	}

	return *library;
}

}

//-----------------------------------------------------------------------------
// Call this to connect to all tier 3 libraries.
// It's up to the caller to check the globals it cares about to see if ones are missing
//-----------------------------------------------------------------------------
void ConnectTier3Libraries( CreateInterfaceFn *pFactoryList, intp nFactoryCount )
{
	// Don't connect twice..
	Assert(!g_pStudioRender && !studiorender &&
		!g_pVGui &&
		!g_pVGuiInput &&
		!g_pVGuiPanel &&
		!g_pVGuiSurface &&
		!g_pVGuiSchemeManager &&
		!g_pVGuiSystem &&
		!g_pVGuiLocalize && 
		!g_pMatSystemSurface &&
		!g_pDataCache &&
		!g_pMDLCache && !mdlcache &&
		!g_pVideo &&
		!g_pDmeMakefileUtils &&
		!g_pPhysicsCollision &&
		!g_pSoundEmitterSystem &&
		!g_pVTex);

	for ( intp i = 0; i < nFactoryCount; ++i )
	{
		studiorender = ConnectLibrary(&g_pStudioRender, pFactoryList, i, STUDIO_RENDER_INTERFACE_VERSION);
		ConnectLibrary(&g_pVGui, pFactoryList, i, VGUI_IVGUI_INTERFACE_VERSION);
		ConnectLibrary(&g_pVGuiInput, pFactoryList, i, VGUI_INPUT_INTERFACE_VERSION);
		ConnectLibrary(&g_pVGuiPanel, pFactoryList, i, VGUI_PANEL_INTERFACE_VERSION);
		ConnectLibrary(&g_pVGuiSurface, pFactoryList, i, VGUI_SURFACE_INTERFACE_VERSION);
		ConnectLibrary(&g_pVGuiSchemeManager, pFactoryList, i, VGUI_SCHEME_INTERFACE_VERSION);
		ConnectLibrary(&g_pVGuiSystem, pFactoryList, i, VGUI_SYSTEM_INTERFACE_VERSION);
		ConnectLibrary(&g_pVGuiLocalize, pFactoryList, i, VGUI_LOCALIZE_INTERFACE_VERSION);
		ConnectLibrary(&g_pMatSystemSurface, pFactoryList, i, MAT_SYSTEM_SURFACE_INTERFACE_VERSION);
		ConnectLibrary(&g_pDataCache, pFactoryList, i, DATACACHE_INTERFACE_VERSION);
		mdlcache = ConnectLibrary(&g_pMDLCache, pFactoryList, i, MDLCACHE_INTERFACE_VERSION);
		ConnectLibrary(&g_pVideo, pFactoryList, i, VIDEO_SERVICES_INTERFACE_VERSION);
		ConnectLibrary(&g_pDmeMakefileUtils, pFactoryList, i, DMEMAKEFILE_UTILS_INTERFACE_VERSION);
		ConnectLibrary(&g_pPhysicsCollision, pFactoryList, i, VPHYSICS_COLLISION_INTERFACE_VERSION);
		ConnectLibrary(&g_pSoundEmitterSystem, pFactoryList, i, SOUNDEMITTERSYSTEM_INTERFACE_VERSION);
		ConnectLibrary(&g_pVTex, pFactoryList, i, IVTEX_VERSION_STRING);
	}
}

void DisconnectTier3Libraries()
{
	// dimhotepus: In reverse order.
	g_pVTex = nullptr;
	g_pSoundEmitterSystem = nullptr;
	g_pPhysicsCollision = nullptr;
	g_pDmeMakefileUtils = nullptr;
	g_pVideo = nullptr;
	mdlcache = nullptr;
	g_pMDLCache = nullptr;
	g_pDataCache = nullptr;
	g_pMatSystemSurface = nullptr;
	g_pVGuiLocalize = nullptr;
	g_pVGuiSystem = nullptr;
	g_pVGuiSchemeManager = nullptr;
	g_pVGuiSurface = nullptr;
	g_pVGuiPanel = nullptr;
	g_pVGuiInput = nullptr;
	g_pVGui = nullptr;
	studiorender = nullptr;
	g_pStudioRender = nullptr;
}
