//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//
//=============================================================================//

#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)

#include "vallocator.h"
#include "basetypes.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

VStdAllocator g_StdAllocator;

void* VStdAllocator::Alloc(size_t size) {
  if (size) {
    return malloc(size);
  }

  return nullptr;
}

void VStdAllocator::Free(void* ptr) { free(ptr); }

#endif  // !_STATIC_LINKED || _SHARED_LIB
