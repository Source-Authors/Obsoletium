// Copyright Valve Corporation, All rights reserved.
//
// This should contain ONLY general purpose macros that are
// appropriate for use in engine/launcher/all tools

#ifndef VPC_TIER0_COMMONMACROS_H_
#define VPC_TIER0_COMMONMACROS_H_

// Makes a 4-byte "packed ID" int out of 4 characters
#define MAKEID(d, c, b, a) \
  (((int)(a) << 24) | ((int)(b) << 16) | ((int)(c) << 8) | ((int)(d)))

// Compares a string with a 4-byte packed ID constant
#define STRING_MATCHES_ID(p, id) ((*((int *)(p)) == (id)) ? true : false)
#define ID_TO_STRING(id, p)                                        \
  ((p)[3] = (((id) >> 24) & 0xFF), (p)[2] = (((id) >> 16) & 0xFF), \
   (p)[1] = (((id) >> 8) & 0xFF), (p)[0] = (((id) >> 0) & 0xFF))

#define V_ARRAYSIZE(p) (sizeof(p) / sizeof(p[0]))

#define SETBITS(iBitVector, bits) ((iBitVector) |= (bits))
#define CLEARBITS(iBitVector, bits) ((iBitVector) &= ~(bits))
#define FBitSet(iBitVector, bits) ((iBitVector) & (bits))

inline bool IsPowerOfTwo(int value) { return (value & (value - 1)) == 0; }

#ifndef REFERENCE
#define REFERENCE(arg) ((void)arg)
#endif

// Wraps the integer in quotes, allowing us to form constant strings with
// it
#define CONST_INTEGER_AS_STRING(x) #x
//__LINE__ can only be converted to an actual number by going through
// this, otherwise the output is literally "__LINE__"
#define __HACK_LINE_AS_STRING__(x) CONST_INTEGER_AS_STRING(x)
// Gives you the line number in constant string form
#define __LINE__AS_STRING __HACK_LINE_AS_STRING__(__LINE__)

#endif  // VPC_TIER0_COMMONMACROS_H_
