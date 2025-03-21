//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The abstract base operator class - all actions within the scenegraph happen via operators
//
//=============================================================================

#ifndef DMEOPERATOR_H
#define DMEOPERATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"

#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// A class representing a camera
//-----------------------------------------------------------------------------
class CDmeOperator : public IDmeOperator, public CDmElement
{
	DEFINE_ELEMENT( CDmeOperator, CDmElement );

public:
	bool IsDirty() override; // ie needs to operate
	void Operate() override {}

	void GetInputAttributes ( CUtlVector< CDmAttribute * > & ) override {}
	void GetOutputAttributes( CUtlVector< CDmAttribute * > & ) override {}
};


#endif // DMEOPERATOR_H
