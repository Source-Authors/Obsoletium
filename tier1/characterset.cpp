// Copyright Valve Corporation, All rights reserved.
//
//

#include "tier1/characterset.h"

#include "tier0/commonmacros.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Purpose: builds a simple lookup table of a group of important characters
void CharacterSetBuild(characterset_t *set, const char *source) {
  if (!set || !source) return;

  BitwiseClear(set->set);

  intp i{0};

  while (source[i]) {
    set->set[static_cast<unsigned char>(source[i])] = static_cast<char>(1);

    ++i;
  }
}
