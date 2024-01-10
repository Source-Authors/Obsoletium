//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _SSE_H
#define _SSE_H

float _SSE_Sqrt(float x);
float _SSE_RSqrtAccurate(float a);
float _SSE_RSqrtFast(float x);
float FASTCALL _SSE_VectorNormalize(Vector& vec);
void FASTCALL _SSE_VectorNormalizeFast(Vector& vec);
float _SSE_InvRSquared(const float* v);

#endif // _SSE_H
