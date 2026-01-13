//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           ViewerSettings.cpp
// last modified:  May 29 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
// version:        1.2
//
// email:          mete@swissquake.ch
// web:            https://www.chumba.ch/chumbalum-soft/hlmv/index.html
//
#include "ViewerSettings.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>



ViewerSettings g_viewerSettings;



void
InitViewerSettings (void)
{
	memset (&g_viewerSettings, 0, sizeof (ViewerSettings));
	g_viewerSettings.rot[0] = -90.0f;
	//g_viewerSettings.trans[3] = 50.0f;
	g_viewerSettings.renderMode = RM_TEXTURED;
	g_viewerSettings.transparency = 1.0f;

	g_viewerSettings.gColor[0] = 0.85f;
	g_viewerSettings.gColor[1] = 0.85f;
	g_viewerSettings.gColor[2] = 0.69f;

	g_viewerSettings.lColor[0] = 1.0f;
	g_viewerSettings.lColor[1] = 1.0f;
	g_viewerSettings.lColor[2] = 1.0f;

	g_viewerSettings.speedScale = 1.0f;
	g_viewerSettings.textureLimit = 256;

	g_viewerSettings.textureScale = 1.0f;
}



int
LoadViewerSettings (const char *filename)
{
	FILE *file = fopen (filename, "rb");
	if (!file)
		return 0;

	RunCodeAtScopeExit(fclose (file) );

	fread (&g_viewerSettings, sizeof (ViewerSettings), 1, file);

	return 1;
}



int
SaveViewerSettings (const char *filename)
{
	FILE *file = fopen (filename, "wb");
	if (!file)
		return 0;

	RunCodeAtScopeExit(fclose (file) );

	fwrite (&g_viewerSettings, sizeof (ViewerSettings), 1, file);

	return 1;
}
