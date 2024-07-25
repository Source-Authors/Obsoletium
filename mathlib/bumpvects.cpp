//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)

#include "mathlib/bumpvects.h"
#include "mathlib/vector.h"
#include "mathlib/ssemath.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static const DirectX::XMVECTOR g_localBumpBasis2[NUM_BUMP_VECTS] = 
{
	DirectX::XMVectorSet( OO_SQRT_2_OVER_3, 0.0f,       OO_SQRT_3, 0.0f ),
	DirectX::XMVectorSet( -OO_SQRT_6,       OO_SQRT_2,  OO_SQRT_3, 0.0f ),
	DirectX::XMVectorSet( -OO_SQRT_6,       -OO_SQRT_2, OO_SQRT_3, 0.0f )
};

// z is coming out of the face.

void XM_CALLCONV GetBumpNormals( Vector sVect, Vector tVect, const Vector& flatNormal, 
					 const Vector& phongNormal, Vector (&bumpNormals)[NUM_BUMP_VECTS] )
{
	DirectX::XMVECTOR vsVect = DirectX::XMLoadFloat3( sVect.XmBase() );
	DirectX::XMVECTOR vtVect = DirectX::XMLoadFloat3( tVect.XmBase() );
	DirectX::XMVECTOR vflatNormal = DirectX::XMLoadFloat3( flatNormal.XmBase() );
	
	// Are we left or right handed?
	DirectX::XMVECTOR tmpNormal = DirectX::XMVector3Cross( vsVect, vtVect );
	const bool leftHanded = DirectX::XMVector3Less( DirectX::XMVector3Dot( vflatNormal, tmpNormal ), DirectX::g_XMZero );

	// Build a basis for the face around the phong normal
	DirectX::XMVECTOR vphongNormal = DirectX::XMLoadFloat3( phongNormal.XmBase() );
	matrix3x4_t smoothBasis;

	DirectX::XMVECTOR vsmooth1 = DirectX::XMVector3Cross( vphongNormal, vsVect );
	VectorNormalize( vsmooth1 );

	DirectX::XMVECTOR vsmooth2 = DirectX::XMVector3Cross( vsmooth1, vphongNormal );
	VectorNormalize( vsmooth2 );

	DirectX::XMStoreFloat4( smoothBasis.XmBase(), vsmooth2 );

	if( leftHanded )
	{
		vsmooth1 = DirectX::XMVectorNegate( vsmooth1 );
	}

	DirectX::XMStoreFloat4( smoothBasis.XmBase() + 1, vsmooth1 );
	DirectX::XMStoreFloat4( smoothBasis.XmBase() + 2, vphongNormal );
	
	static_assert(std::size(g_localBumpBasis2) == NUM_BUMP_VECTS);
	static_assert(ARRAYSIZE(bumpNormals) == NUM_BUMP_VECTS);

	// move the g_localBumpBasis2 into world space to create bumpNormals
	DirectX::XMVECTOR n0 = VectorIRotate( g_localBumpBasis2[0], smoothBasis );
	DirectX::XMVECTOR n1 = VectorIRotate( g_localBumpBasis2[1], smoothBasis );
	DirectX::XMVECTOR n2 = VectorIRotate( g_localBumpBasis2[2], smoothBasis );

	DirectX::XMStoreFloat3( bumpNormals[0].XmBase(), n0 );
	DirectX::XMStoreFloat3( bumpNormals[1].XmBase(), n1 );
	DirectX::XMStoreFloat3( bumpNormals[2].XmBase(), n2 );
}

#endif // !_STATIC_LINKED || _SHARED_LIB
