// Copyright Valve Corporation, All rights reserved.
//
// Purpose: Shared code for parsing / searching for characters in a string using
// lookup tables

#ifndef VPC_TIER1_CHARACTERSET_H_
#define VPC_TIER1_CHARACTERSET_H_

struct characterset_t {
  char set[256];
};

// This is essentially a strpbrk() using a precalculated lookup table
// 
// Purpose: builds a simple lookup table of a group of important characters
// Input  : *pSetBuffer - pointer to the buffer for the group
//			*pSetString - list of characters to flag
extern void CharacterSetBuild(characterset_t *pSetBuffer,
                              const char *pSetString);

// Input  : *pSetBuffer - pre-build group buffer
//			character - character to lookup
// Output : int - 1 if the character was in the set
#define IN_CHARACTERSET(SetBuffer, character) \
  ((SetBuffer).set[(unsigned char)(character)])

#endif  // VPC_TIER1_CHARACTERSET_H_
