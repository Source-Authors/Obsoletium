//========= Copyright Valve Corporation, All rights reserved. ============//
//                       TOGL CODE LICENSE
//
//  Copyright 2011-2014 Valve Corporation
//  All Rights Reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
#ifndef GLMDISPLAYDB_H
#define	GLMDISPLAYDB_H

#include "tier0/platform.h"
#include "tier1/utlvector.h"

// dimhotepus: NOTE: glmdisplay.h (GLM*InfoFields) must be included before this header.

//===============================================================================
// modes, displays, and renderers
//===============================================================================

// GLMDisplayModeInfoFields is in glmdisplay.h

class GLMDisplayMode
{
public:
	GLMDisplayModeInfoFields	m_info;
	
	GLMDisplayMode( uint width, uint height, uint refreshHz );
	// dimhotepus: Zero the fields, they were left uninitialized.
	GLMDisplayMode() : m_info{} { }
	~GLMDisplayMode( void );

	void	Init( uint width, uint height, uint refreshHz );
	void	Dump( int which );
};

//===============================================================================

// GLMDisplayInfoFields is in glmdisplay.h

class GLMDisplayInfo
{
public:
	// dimhotepus: Default member initializers, so no constructor can leave them uninitialized.
	GLMDisplayInfoFields			m_info{};
	CUtlVector< GLMDisplayMode* >	*m_modes{nullptr};		// starts out NULL, set by PopulateModes
	GLMDisplayMode					m_DesktopMode;

#ifdef OSX
	GLMDisplayInfo( CGDirectDisplayID displayID, CGOpenGLDisplayMask displayMask );
#else
	GLMDisplayInfo( void );
#endif

	~GLMDisplayInfo( void );

	// dimhotepus: Owns m_modes (deleted in destructor), copying would double delete.
	GLMDisplayInfo( const GLMDisplayInfo & ) = delete;
	GLMDisplayInfo &operator=( const GLMDisplayInfo & ) = delete;
	
	void	PopulateModes( void );

	void	Dump( int which );

#ifdef OSX
private:
	int m_display{0};
#endif
};

//===============================================================================

// GLMRendererInfoFields is in glmdisplay.h

class GLMRendererInfo
{
public:
	// dimhotepus: Default member initializers, so no constructor can leave them uninitialized.
	GLMRendererInfoFields			m_info{};
#ifdef OSX
	CUtlVector< GLMDisplayInfo* >	*m_displays{nullptr};	// starts out NULL, set by PopulateDisplays
#else
	GLMDisplayInfo					*m_display{nullptr};
#endif

#ifdef OSX
	// dimhotepus: explicit, and const info, it is only read.
	explicit GLMRendererInfo( const GLMRendererInfoFields *info );
#else
	GLMRendererInfo			();
#endif
	~GLMRendererInfo		( void );

	// dimhotepus: Owns displays (deleted in destructor), copying would double delete.
	GLMRendererInfo( const GLMRendererInfo & ) = delete;
	GLMRendererInfo &operator=( const GLMRendererInfo & ) = delete;

#ifndef OSX
	void	Init( const GLMRendererInfoFields *info );
#endif
	void	PopulateDisplays();
	void	Dump( int which );
};

//===============================================================================

#ifdef OSX
// this is just a tuple describing fake adapters which are really renderer/display pairings.
// dxabstract bridges the gap between the d3d adapter-centric world and the GL renderer+display world.
// this makes it straightforward to handle cases like two video cards with two displays on one, and one on the other -
// you get three fake adapters which represent each useful screen.

// the constraint that dxa will have to follow though, is that if the user wants to change their 
// display selection for full screen, they would only be able to pick on that has the same underlying renderer.
// can't change fakeAdapter from one to another with different GL renderer under it.  Screen hop but no card hop.

struct GLMFakeAdapter
{
	int		m_rendererIndex{0};
	int		m_displayIndex{0};
};
#endif

class GLMDisplayDB
{
public:
#ifdef OSX
	CUtlVector< GLMRendererInfo* >		*m_renderers{nullptr};	// starts out NULL, set by PopulateRenderers
	CUtlVector< GLMFakeAdapter >		m_fakeAdapters;
#else
	GLMRendererInfo	m_renderer;
#endif

	GLMDisplayDB	( void );
	// dimhotepus: Class is polymorphic (virtual methods), so destructor must be virtual.
	virtual ~GLMDisplayDB	( void );

	// dimhotepus: Owns renderers, copying would double delete.
	GLMDisplayDB( const GLMDisplayDB & ) = delete;
	GLMDisplayDB &operator=( const GLMDisplayDB & ) = delete;	

	virtual void	PopulateRenderers( void );
	virtual void	PopulateFakeAdapters( uint realRendererIndex );		// fake adapters = one real adapter times however many displays are on it
	virtual void	Populate( void );
	
	// The info-get functions return false on success.
	virtual	int		GetFakeAdapterCount( void );
	virtual	bool	GetFakeAdapterInfo( int fakeAdapterIndex, int *rendererOut, int *displayOut, GLMRendererInfoFields *rendererInfoOut, GLMDisplayInfoFields *displayInfoOut );
	
	virtual	int		GetRendererCount( void );
	virtual	bool	GetRendererInfo( int rendererIndex, GLMRendererInfoFields *infoOut );
	
	virtual	int		GetDisplayCount( int rendererIndex );
	virtual	bool	GetDisplayInfo( int rendererIndex, int displayIndex, GLMDisplayInfoFields *infoOut );

	virtual	int		GetModeCount( int rendererIndex, int displayIndex );
	virtual	bool	GetModeInfo( int rendererIndex, int displayIndex, int modeIndex, GLMDisplayModeInfoFields *infoOut );
	
	virtual	void	Dump( void );
};

#endif // GLMDISPLAYDB_H