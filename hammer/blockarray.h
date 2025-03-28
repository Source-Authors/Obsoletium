//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef _BLOCKARRAY_H
#define _BLOCKARRAY_H

#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

template <class T, short nBlockSize, short nMaxBlocks>
class BlockArray
{
public:
	BlockArray()
	{
		memset(Blocks, 0, sizeof(Blocks));
		nCount = nBlocks = 0;
	}
	~BlockArray()
	{
		GetBlocks(0);
	}

	T& operator[] (short iIndex);
	
	void SetCount(short nObjects);
	short GetCount() const { return nCount; }

private:
	T * Blocks[nMaxBlocks+1];
	short nCount;
	short nBlocks;
	void GetBlocks(short nNewBlocks);
};

template <class T, short nBlockSize, short nMaxBlocks>
void BlockArray<T,nBlockSize,nMaxBlocks>::
	GetBlocks(short nNewBlocks)
{
	for(auto i = nBlocks; i < nNewBlocks; i++)
	{
		Blocks[i] = new T[nBlockSize];
	}
	for(auto i = nNewBlocks; i < nBlocks; i++)
	{
		delete[] Blocks[i];
	}

	nBlocks = nNewBlocks;
}

template <class T, short nBlockSize, short nMaxBlocks>
void BlockArray<T,nBlockSize,nMaxBlocks>::
	SetCount(short nObjects)
{
	if(nObjects == nCount)
		return;

	// find the number of blocks required by nObjects, checking for
	// integer rounding error
	short nNewBlocks = nObjects / nBlockSize;
	if (nNewBlocks * nBlockSize < nObjects)
	{
		nNewBlocks++;
	}

	if(nNewBlocks != nBlocks)
	{
		// Make sure we don't get an overrun.
		if ( nNewBlocks > ARRAYSIZE( Blocks ) )
		{
			Error( "BlockArray< ?, %hd, %hd > - too many blocks needed.", nBlockSize, nMaxBlocks );
		}
		
		GetBlocks(nNewBlocks);
	}
	nCount = nObjects;
}

template <class T, short nBlockSize, short nMaxBlocks>
T& BlockArray<T,nBlockSize,nMaxBlocks>::operator[] (short iIndex)
{
	// Cast to unsigned so that this check will reject negative values as
	// well as overly large values.
	if((unsigned)iIndex >= (unsigned)nCount)
	{
		Error( "BlockArray< %hd, %hd > - invalid block index.", iIndex, nCount );
		SetCount(iIndex+1);
	}
	return Blocks[iIndex / nBlockSize][iIndex % nBlockSize];
}

#include <tier0/memdbgoff.h>

#endif // _BLOCKARRAY_H