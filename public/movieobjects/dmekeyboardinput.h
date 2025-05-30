//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The keyboard input class
//
//=============================================================================

#ifndef DMEKEYBOARDINPUT_H
#define DMEKEYBOARDINPUT_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmeinput.h"


//-----------------------------------------------------------------------------
// A class representing a camera
//-----------------------------------------------------------------------------
class CDmeKeyboardInput : public CDmeInput
{
	DEFINE_ELEMENT( CDmeKeyboardInput, CDmeInput );

public:
	bool IsDirty() override; // ie needs to operate
	void Operate() override;

	void GetInputAttributes ( CUtlVector< CDmAttribute * > &attrs ) override;
	void GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs ) override;

protected:
	CDmaVar< bool > *m_keys;

	bool GetKeyStatus( uint ki );
};

#endif // DMEKEYBOARDINPUT_H
