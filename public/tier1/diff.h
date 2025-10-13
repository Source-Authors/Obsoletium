//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// Serialization/unserialization buffer
//=============================================================================//

#ifndef DIFF_H
#define DIFF_H

#include "tier0/basetypes.h"
#include "tier0/wchartypes.h"

int FindDiffs(uint8 const *NewBlock, uint8 const *OldBlock,
			  intp NewSize, intp OldSize, intp &DiffListSize,uint8 *Output,uint32 OutSize);

int FindDiffsForLargeFiles(uint8 const *NewBlock, uint8 const *OldBlock,
						   intp NewSize, intp OldSize, intp &DiffListSize,uint8 *Output,
						   uint32 OutSize,
						   int hashsize=65536);

void ApplyDiffs(uint8 const *OldBlock, uint8 const *DiffList,
                intp OldSize, intp DiffListSize, intp &ResultListSize,uint8 *Output,uint32 OutSize);

int FindDiffsLowMemory(uint8 const *NewBlock, uint8 const *OldBlock,
					   intp NewSize, intp OldSize, intp &DiffListSize,uint8 *Output,uint32 OutSize);

#endif

