//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef LOCALD3DTYPES_H
#define LOCALD3DTYPES_H

#ifdef _WIN32
#pragma once
#endif

#include <cstddef>  // std::ptrdiff_t

#if !defined( _X360 ) && !defined( DX_TO_GL_ABSTRACTION )
#ifdef _DEBUG
#define D3D_DEBUG_INFO 1
#endif
#endif

struct IDirect3DTexture9;
struct IDirect3DBaseTexture9;
struct IDirect3DCubeTexture9;
struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DSurface9;
struct IDirect3DIndexBuffer9;
struct IDirect3DVertexBuffer9;
struct IDirect3DVertexShader9;
struct IDirect3DPixelShader9;
struct IDirect3DVolumeTexture9;

typedef struct _D3DLIGHT9				D3DLIGHT9;
typedef struct _D3DADAPTER_IDENTIFIER9	D3DADAPTER_IDENTIFIER9;
typedef struct _D3DCAPS9				D3DCAPS9;
typedef struct _D3DVIEWPORT9			D3DVIEWPORT9;
typedef struct _D3DMATERIAL9			D3DMATERIAL9;
typedef IDirect3DTexture9				IDirect3DTexture;
typedef IDirect3DBaseTexture9			IDirect3DBaseTexture;
typedef IDirect3DCubeTexture9			IDirect3DCubeTexture;
typedef IDirect3DVolumeTexture9 		IDirect3DVolumeTexture;
typedef IDirect3DDevice9				IDirect3DDevice;
typedef D3DMATERIAL9					D3DMATERIAL;
typedef D3DLIGHT9						D3DLIGHT;
typedef IDirect3DSurface9				IDirect3DSurface;
typedef D3DCAPS9						D3DCAPS;
typedef IDirect3DIndexBuffer9			IDirect3DIndexBuffer;
typedef IDirect3DVertexBuffer9			IDirect3DVertexBuffer;
typedef IDirect3DPixelShader9			IDirect3DPixelShader;
typedef IDirect3DDevice					*LPDIRECT3DDEVICE;
typedef IDirect3DIndexBuffer			*LPDIRECT3DINDEXBUFFER;
typedef IDirect3DVertexBuffer			*LPDIRECT3DVERTEXBUFFER;

typedef void *HardwareShader_t;

//-----------------------------------------------------------------------------
// The vertex and pixel shader type
//-----------------------------------------------------------------------------
typedef std::ptrdiff_t VertexShader_t;
typedef std::ptrdiff_t PixelShader_t;

//-----------------------------------------------------------------------------
// Bitpattern for an invalid shader
//-----------------------------------------------------------------------------
#define INVALID_SHADER	-1 // ( 0xFFFFFFFF )
#define INVALID_HARDWARE_SHADER ( NULL )

#define D3DSAMP_NOTSUPPORTED					D3DSAMP_FORCE_DWORD
#define D3DRS_NOTSUPPORTED						D3DRS_FORCE_DWORD

#include "togl/rendermechanism.h"

#endif // LOCALD3DTYPES_H
