//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CLIENT_RENDER_HANDLE_H
#define CLIENT_RENDER_HANDLE_H
#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Foward declarations
//-----------------------------------------------------------------------------
class IClientRenderable;


//-----------------------------------------------------------------------------
// Handle to an renderable in the client leaf system
//-----------------------------------------------------------------------------
// dimhotepus: Make unsigned int as expected.
typedef unsigned int ClientRenderHandle_t;

enum : ClientRenderHandle_t
{
	INVALID_CLIENT_RENDER_HANDLE = (ClientRenderHandle_t)~0,
};


#endif // CLIENT_RENDER_HANDLE_H
