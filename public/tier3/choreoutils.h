//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper methods + classes for choreo
//
//===========================================================================//

#ifndef CHOREOUTILS_H
#define CHOREOUTILS_H

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CChoreoScene;
class CChoreoEvent;
class CStudioHdr;


//-----------------------------------------------------------------------------
// Finds sound files associated with events
//-----------------------------------------------------------------------------
[[nodiscard]] const char *GetSoundForEvent( CChoreoEvent *pEvent, CStudioHdr *pStudioHdr );


//-----------------------------------------------------------------------------
// Fixes up the duration of a choreo scene based on wav files + animations
// Returns true if a change needed to be made
//-----------------------------------------------------------------------------
[[nodiscard]] bool AutoAddGestureKeys( CChoreoEvent *e, CStudioHdr *pStudioHdr, float *pPoseParameters, bool bCheckOnly );
[[nodiscard]] bool UpdateGestureLength( CChoreoEvent *e, CStudioHdr *pStudioHdr, float *pPoseParameters, bool bCheckOnly );
[[nodiscard]] bool UpdateSequenceLength( CChoreoEvent *e, CStudioHdr *pStudioHdr, float *pPoseParameters, bool bCheckOnly, bool bVerbose );


#endif // CHOREOUTILS_H

