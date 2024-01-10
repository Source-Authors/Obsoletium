//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _SSE_H
#define _SSE_H

float FASTCALL _SSE_VectorNormalize(Vector& vec);
void FASTCALL _SSE_VectorNormalizeFast(Vector& vec);
float _SSE_InvRSquared(const float* v);

#endif // _SSE_H
