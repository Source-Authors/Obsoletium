//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef SCENETOKENPROCESSOR_H
#define SCENETOKENPROCESSOR_H

class ISceneTokenProcessor;

[[nodiscard]] ISceneTokenProcessor *GetTokenProcessor();
void SetTokenProcessorBuffer( const char *buf );

#endif // SCENETOKENPROCESSOR_H
