//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The abstract base operator class - all actions within the scenegraph happen via operators
//
//=============================================================================

#ifndef DMEINPUT_H
#define DMEINPUT_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmeoperator.h"


//-----------------------------------------------------------------------------
// A class representing a camera
//-----------------------------------------------------------------------------
class CDmeInput : public CDmeOperator
{
	DEFINE_ELEMENT( CDmeInput, CDmeOperator );

public:
	bool IsDirty() override; // ie needs to operate
};


#endif // DMEINPUT_H
