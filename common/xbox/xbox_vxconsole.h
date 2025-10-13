//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: XBox VXConsole Common. Used for public remote access items.
//
//=============================================================================
#pragma once

// sent during connection, used to explicitly guarantee a binary compatibility
enum {
VXCONSOLE_PROTOCOL_VERSION =	103
};

using xrProfile_t = struct
{
	char		labelString[128];
	COLORREF	color;
};

using xrTimeStamp_t = struct
{
	char		messageString[256];
	float		time;
	float		deltaTime;
	int			memory;
	int			deltaMemory;
};

using xrMaterial_t = struct
{
	char		nameString[256];
	char		shaderString[256];
	int			refCount;
};

using xrTexture_t = struct
{
	char		nameString[256];
	char		groupString[64];
	char		formatString[64];
	int			size;
	int			width;
	int			height;
	int			depth;
	int			numLevels;
	int			binds;
	int			refCount;
	int			sRGB;
	int			edram;
	int			procedural;
	int			fallback;
	int			final;
	int			failed;
};

using xrSound_t = struct
{
	char		nameString[256];
	char		formatString[64];
	int			rate;
	int			bits;
	int			channels;
	int			looped;
	int			dataSize;
	int			numSamples;
	int			streamed;
};

using xrCommand_t = struct
{
	char		nameString[128];
	char		helpString[256];	
};

using xrMapInfo_t = struct
{
	float	position[3];
	float	angle[3];
	char	mapPath[256];
	char	savePath[256];
	int		build;
	int		skill;
};

// Types of action taken in response to an rc_Assert() message
enum AssertAction_t
{
	ASSERT_ACTION_BREAK = 0,		//	Break on this Assert
	ASSERT_ACTION_IGNORE_THIS,		//	Ignore this Assert once
	ASSERT_ACTION_IGNORE_ALWAYS,	//	Ignore this Assert from now on
	ASSERT_ACTION_IGNORE_FILE,		//	Ignore all Asserts from this file from now on
	ASSERT_ACTION_IGNORE_ALL,		//	Ignore all Asserts from now on
	ASSERT_ACTION_OTHER				//	A more complex response requiring additional data (e.g. "ignore this Assert 5 times")
};
