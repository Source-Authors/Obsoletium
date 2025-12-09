//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//===========================================================================//

#ifndef SE_VSCRIPT_LANGUAGES_SQUIRREL_VSQUIRREL_VSQ_VEC3_H_
#define SE_VSCRIPT_LANGUAGES_SQUIRREL_VSQUIRREL_VSQ_VEC3_H_

#include "mathlib/vector.h"
#include "squirrel.h"
#include "sqplus.h"

const inline SQUserPointer TYPETAG_VECTOR{(SQUserPointer)1}; //-V566
constexpr inline char TYPENAME_VECTOR[]{"Vector"};

SQInteger vsq_openvec3(HSQUIRRELVM hVM, HSQOBJECT *hExternalClass);
SQInteger vsq_releasevec3(SQUserPointer p, SQInteger size);

SQInteger vsq_getvec3(HSQUIRRELVM hVM, StackHandler &sh, intp idx, Vector &vec);

#endif  // !SE_VSCRIPT_LANGUAGES_SQUIRREL_VSQUIRREL_VSQ_VEC3_H_
