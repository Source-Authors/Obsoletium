//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef VECTOR4D_H
#define VECTOR4D_H

#ifdef _WIN32
#pragma once
#endif

#include <DirectXMath.h>

#include <cmath>
#include <cstdlib>  // rand

#include "tier0/basetypes.h" // vec_t
#include "tier0/dbg.h"
#include "tier0/memalloc.h"  // CAlignedNewDelete<X>

#include "mathlib/vector.h"
#include "mathlib/math_pfns.h"

// forward declarations
class Vector;
class Vector2D;

//=========================================================
// 4D Vector4D
//=========================================================

// dimhotepus: alignas to use vec_t operator[] safely.
// dimhotepus: XM_CALLCONV to speedup calls.
class alignas(vec_t) Vector4D
{
public:
	// Members
	vec_t x, y, z, w;

	// Construction/destruction
#ifdef _DEBUG
	Vector4D()
		: x{VEC_T_NAN}, y{VEC_T_NAN}, z{VEC_T_NAN}, w{VEC_T_NAN}
	{
	}
#else
	Vector4D() = default;
#endif

	Vector4D(vec_t X, vec_t Y, vec_t Z, vec_t W);
	Vector4D(const float *pFloat);

	// Initialization
	void XM_CALLCONV Init(vec_t ix=0.0f, vec_t iy=0.0f, vec_t iz=0.0f, vec_t iw=0.0f);

	// Got any nasty NAN's?
	bool XM_CALLCONV IsValid() const;

	// array access...
	vec_t XM_CALLCONV operator[](int i) const;
	vec_t& XM_CALLCONV operator[](int i);

	// Base address...
	inline vec_t* XM_CALLCONV Base();
	inline vec_t const* XM_CALLCONV Base() const;

	// dimhotepus: Better DirectX math integration.
	DirectX::XMFLOAT4* XM_CALLCONV XmBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT4) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT4) == alignof(Vector4D));
		return reinterpret_cast<DirectX::XMFLOAT4*>(this);
	}
	DirectX::XMFLOAT4 const* XM_CALLCONV XmBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT4) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT4) == alignof(Vector4D));
		return reinterpret_cast<DirectX::XMFLOAT4 const*>(this);
	}

	// Cast to Vector and Vector2D...
	Vector& XM_CALLCONV AsVector3D();
	Vector const& XM_CALLCONV AsVector3D() const;

	Vector2D& XM_CALLCONV AsVector2D();
	Vector2D const& XM_CALLCONV AsVector2D() const;

	// Initialization methods
	void XM_CALLCONV Random( vec_t minVal, vec_t maxVal );

	// equality
	bool XM_CALLCONV operator==(const Vector4D& v) const;
	bool XM_CALLCONV operator!=(const Vector4D& v) const;

	// arithmetic operations
	Vector4D XM_CALLCONV operator+(const Vector4D &v) const;
	Vector4D& XM_CALLCONV operator+=(const Vector4D &v);
	Vector4D XM_CALLCONV operator-(const Vector4D &v) const;
	Vector4D& XM_CALLCONV operator-=(const Vector4D &v);
	Vector4D XM_CALLCONV operator*(const Vector4D &v) const;
	Vector4D& XM_CALLCONV operator*=(const Vector4D &v);
	Vector4D XM_CALLCONV operator*(float s) const;
	Vector4D& XM_CALLCONV operator*=(float s);
	Vector4D XM_CALLCONV operator/(const Vector4D &v) const;
	Vector4D& XM_CALLCONV operator/=(const Vector4D &v);
	Vector4D XM_CALLCONV operator/(float s) const;
	Vector4D& XM_CALLCONV operator/=(float s);

	// negate the Vector4D components
	void	Negate();

	// Get the Vector4D's magnitude.
	vec_t XM_CALLCONV Length() const;

	// Get the Vector4D's magnitude squared.
	vec_t XM_CALLCONV LengthSqr() const;

	// return true if this vector is (0,0,0,0) within tolerance
	bool XM_CALLCONV IsZero( float tolerance = 0.01f ) const
	{
		return DirectX::XMVector3NearEqual
		(
			DirectX::g_XMZero,
			DirectX::XMLoadFloat4( XmBase() ),
			DirectX::XMVectorReplicate( tolerance ) //-V2002
		);
	}

	// Get the distance from this Vector4D to the other one.
	vec_t XM_CALLCONV DistTo(const Vector4D &vOther) const;

	// Get the distance from this Vector4D to the other one squared.
	vec_t XM_CALLCONV DistToSqr(const Vector4D &vOther) const;

	// Copy
	void XM_CALLCONV CopyToArray(float* rgfl) const;

	// Multiply, add, and assign to this (ie: *this = a + b * scalar). This
	// is about 12% faster than the actual Vector4D equation (because it's done per-component
	// rather than per-Vector4D).
	[[deprecated("Use Vector4DMA")]] void XM_CALLCONV MulAdd(Vector4D const& a, Vector4D const& b, float scalar);

	// Dot product.
	vec_t XM_CALLCONV Dot(Vector4D const& vOther) const;

	// No copy constructors allowed if we're in optimal mode
#ifdef VECTOR_NO_SLOW_OPERATIONS
private:
#else
public:
#endif
	Vector4D(Vector4D const& vOther);

	// No assignment operators either...
	Vector4D& operator=( Vector4D const& src );
};

// dimhotepus: inline
const inline Vector4D vec4_origin( 0.0f, 0.0f, 0.0f, 0.0f );
// dimhotepus: inline
const inline Vector4D vec4_invalid( FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX );

//-----------------------------------------------------------------------------
// SSE optimized routines
//-----------------------------------------------------------------------------
// dimhotepus: Fix aligned alloc.
class alignas(16) Vector4DAligned : public CAlignedNewDelete<16, Vector4D>
{
public:
	Vector4DAligned() = default;
	Vector4DAligned( vec_t X, vec_t Y, vec_t Z, vec_t W );

	inline void XM_CALLCONV Set(vec_t X, vec_t Y, vec_t Z, vec_t W);
	inline void XM_CALLCONV InitZero();

	// dimhotepus: Unsafe cast. Use XmBase.
	inline DirectX::XMVECTOR& XM_CALLCONV AsM128() = delete; // { return *(DirectX::XMVECTOR*)&x; }
	// dimhotepus: Unsafe cast. Use XmBase.
	inline const DirectX::XMVECTOR& XM_CALLCONV AsM128() const = delete; // { return *(const DirectX::XMVECTOR*)&x; }

	// dimhotepus: Better DirectX math integration.
	DirectX::XMFLOAT4A* XM_CALLCONV XmBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT4A) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT4A) == alignof(Vector4DAligned));
		return reinterpret_cast<DirectX::XMFLOAT4A*>(this);
	}
	DirectX::XMFLOAT4A const* XM_CALLCONV XmBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT4A) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT4A) == alignof(Vector4DAligned));
		return reinterpret_cast<DirectX::XMFLOAT4A const*>(this);
	}

private:
	// No copy constructors allowed if we're in optimal mode
	Vector4DAligned( Vector4DAligned const& vOther ) = delete;

	// No assignment operators either...
	Vector4DAligned& operator=( Vector4DAligned const& src ) = delete;
};

//-----------------------------------------------------------------------------
// Vector4D related operations
//-----------------------------------------------------------------------------

// Vector4D clear
void XM_CALLCONV Vector4DClear(Vector4D& a);

// Copy
void XM_CALLCONV Vector4DCopy(Vector4D const& src, Vector4D& dst);

// Vector4D arithmetic
void XM_CALLCONV Vector4DAdd( Vector4D const& a, Vector4D const& b, Vector4D& result );
void XM_CALLCONV Vector4DSubtract( Vector4D const& a, Vector4D const& b, Vector4D& result );
void XM_CALLCONV Vector4DMultiply( Vector4D const& a, vec_t b, Vector4D& result );
void XM_CALLCONV Vector4DMultiply( Vector4D const& a, Vector4D const& b, Vector4D& result );
void XM_CALLCONV Vector4DDivide( Vector4D const& a, vec_t b, Vector4D& result );
void XM_CALLCONV Vector4DDivide( Vector4D const& a, Vector4D const& b, Vector4D& result );
void XM_CALLCONV Vector4DMA( Vector4D const& start, float s, Vector4D const& dir, Vector4D& result );

#define Vector4DExpand( v ) (v).x, (v).y, (v).z, (v).w

// Normalization
vec_t XM_CALLCONV Vector4DNormalize(Vector4D& v);

// Length
vec_t XM_CALLCONV Vector4DLength(Vector4D const& v);

// Dot Product
vec_t XM_CALLCONV DotProduct4D(Vector4D const& a, Vector4D const& b);

// Linearly interpolate between two vectors
void XM_CALLCONV Vector4DLerp(Vector4D const& src1, Vector4D const& src2, vec_t t, Vector4D& dest );


//-----------------------------------------------------------------------------
//
// Inlined Vector4D methods
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// constructors
//-----------------------------------------------------------------------------

inline Vector4D::Vector4D(vec_t X, vec_t Y, vec_t Z, vec_t W )
    : x{X}, y{Y}, z{Z}, w{W}
{ 
	Assert( IsValid() );
}

inline Vector4D::Vector4D(const float *pFloat)
{
	Assert( pFloat );
	x = pFloat[0]; y = pFloat[1]; z = pFloat[2]; w = pFloat[3];
	Assert( IsValid() );
}


//-----------------------------------------------------------------------------
// copy constructor
//-----------------------------------------------------------------------------

inline Vector4D::Vector4D(const Vector4D &vOther)
    : x{vOther.x}, y{vOther.y}, z{vOther.z}, w{vOther.w}
{ 
	Assert( vOther.IsValid() );
}

//-----------------------------------------------------------------------------
// initialization
//-----------------------------------------------------------------------------

inline void Vector4D::Init( vec_t ix, vec_t iy, vec_t iz, vec_t iw )
{ 
	x = ix; y = iy; z = iz;	w = iw;
	Assert( IsValid() );
}

inline void Vector4D::Random( vec_t minVal, vec_t maxVal )
{
	DirectX::XMVECTOR rn = DirectX::XMVectorSet
	(
		(static_cast<float>(rand()) / VALVE_RAND_MAX),
		(static_cast<float>(rand()) / VALVE_RAND_MAX),
		(static_cast<float>(rand()) / VALVE_RAND_MAX),
		(static_cast<float>(rand()) / VALVE_RAND_MAX)
	);
	DirectX::XMVECTOR mn = DirectX::XMVectorReplicate( minVal ); //-V2002

	DirectX::XMStoreFloat4
	(
		XmBase(),
		DirectX::XMVectorMultiplyAdd
		(
			rn,
			DirectX::XMVectorSubtract( DirectX::XMVectorReplicate( maxVal ), mn ), //-V2002
			mn
		)
	);
}

inline void XM_CALLCONV Vector4DClear( Vector4D& a )
{
	a.x = a.y = a.z = a.w = 0.0f;
}

//-----------------------------------------------------------------------------
// assignment
//-----------------------------------------------------------------------------

inline Vector4D& Vector4D::operator=(const Vector4D &vOther)
{
	Assert( vOther.IsValid() );
	x=vOther.x; y=vOther.y; z=vOther.z; w=vOther.w;
	return *this; 
}

//-----------------------------------------------------------------------------
// Array access
//-----------------------------------------------------------------------------

inline vec_t& Vector4D::operator[](int i)
{
	Assert( (i >= 0) && (i < 4) ); //-V112
	static_assert(alignof(Vector4D) == alignof(vec_t));
	return reinterpret_cast<vec_t*>(this)[i];
}

inline vec_t Vector4D::operator[](int i) const
{
	Assert( (i >= 0) && (i < 4) ); //-V112
	static_assert(alignof(Vector4D) == alignof(vec_t));
	return reinterpret_cast<const vec_t*>(this)[i];
}

//-----------------------------------------------------------------------------
// Cast to Vector and Vector2D...
//-----------------------------------------------------------------------------

inline Vector& Vector4D::AsVector3D()
{
	static_assert(alignof(Vector) == alignof(Vector4D));
	return *reinterpret_cast<Vector*>(this);
}

inline Vector const& Vector4D::AsVector3D() const
{
	static_assert(alignof(Vector) == alignof(Vector4D));
	return *reinterpret_cast<const Vector*>(this);
}

inline Vector2D& Vector4D::AsVector2D()
{
	static_assert(alignof(Vector2D) == alignof(Vector4D));
	return *reinterpret_cast<Vector2D*>(this);
}

inline Vector2D const& Vector4D::AsVector2D() const
{
	static_assert(alignof(Vector2D) == alignof(Vector4D));
	return *reinterpret_cast<const Vector2D*>(this);
}

//-----------------------------------------------------------------------------
// Base address...
//-----------------------------------------------------------------------------

inline vec_t* Vector4D::Base()
{
	static_assert(alignof(Vector4D) == alignof(vec_t));
	return reinterpret_cast<vec_t*>(this);
}

inline vec_t const* Vector4D::Base() const
{
	static_assert(alignof(Vector4D) == alignof(vec_t));
	return reinterpret_cast<const vec_t*>(this);
}

//-----------------------------------------------------------------------------
// IsValid?
//-----------------------------------------------------------------------------

inline bool Vector4D::IsValid() const
{
	return !DirectX::XMVector4IsNaN( DirectX::XMLoadFloat4( XmBase() ) );
}

//-----------------------------------------------------------------------------
// comparison
//-----------------------------------------------------------------------------

inline bool Vector4D::operator==( Vector4D const& src ) const
{
	Assert( src.IsValid() && IsValid() );
	return DirectX::XMVector4Equal
	(
		DirectX::XMLoadFloat4( XmBase() ),
		DirectX::XMLoadFloat4( src.XmBase() )
	);
}

inline bool Vector4D::operator!=( Vector4D const& src ) const
{
	return !(*this == src);
}


//-----------------------------------------------------------------------------
// Copy
//-----------------------------------------------------------------------------

inline void XM_CALLCONV Vector4DCopy( Vector4D const& src, Vector4D& dst )
{
	Assert( src.IsValid() );
	dst.x = src.x;
	dst.y = src.y;
	dst.z = src.z;
	dst.w = src.w;
}

inline void	Vector4D::CopyToArray(float* rgfl) const
{ 
	Assert( IsValid() );
	Assert( rgfl );
	rgfl[0] = x; rgfl[1] = y; rgfl[2] = z; rgfl[3] = w;
}

//-----------------------------------------------------------------------------
// standard math operations
//-----------------------------------------------------------------------------

inline void Vector4D::Negate()
{ 
	Assert( IsValid() );
	x = -x; y = -y; z = -z; w = -w;
} 

inline Vector4D Vector4D::operator+(const Vector4D& v) const
{
	Vector4D r{x, y, z, w};
	return r += v;
}

inline Vector4D& Vector4D::operator+=(const Vector4D& v)
{ 
	Assert( IsValid() && v.IsValid() );

	DirectX::XMStoreFloat4
	(
		XmBase(),
		DirectX::XMVectorAdd
		(
			DirectX::XMLoadFloat4( XmBase() ),
			DirectX::XMLoadFloat4( v.XmBase() )
		)
	);

	return *this;
}

inline Vector4D Vector4D::operator-(const Vector4D& v) const
{
	Vector4D r{x, y, z, w};
	return r -= v;
}

inline Vector4D& Vector4D::operator-=(const Vector4D& v)
{ 
	Assert( IsValid() && v.IsValid() );

	DirectX::XMStoreFloat4
	(
		XmBase(),
		DirectX::XMVectorSubtract
		(
			DirectX::XMLoadFloat4( XmBase() ),
			DirectX::XMLoadFloat4( v.XmBase() )
		)
	);

	return *this;
}

inline Vector4D Vector4D::operator*(float v) const
{
	Vector4D r{x, y, z, w};
	return r *= v;
}

inline Vector4D& Vector4D::operator*=(float fl)
{
	DirectX::XMStoreFloat4
	(
		XmBase(),
		DirectX::XMVectorScale
		(
			DirectX::XMLoadFloat4( XmBase() ),
			fl
		)
	);

	Assert( IsValid() );
	return *this;
}

inline Vector4D Vector4D::operator*(Vector4D const& v) const
{
	Vector4D r{x, y, z, w};
	return r *= v;
}

inline Vector4D& Vector4D::operator*=(Vector4D const& v)
{ 
	DirectX::XMStoreFloat4
	(
		XmBase(),
		DirectX::XMVectorMultiply
		(
			DirectX::XMLoadFloat4( XmBase() ),
			DirectX::XMLoadFloat4( v.XmBase() )
		)
	);

	Assert( IsValid() );
	return *this;
}

inline Vector4D Vector4D::operator/(float v) const
{
	Vector4D r{x, y, z, w};
	return r /= v;
}

inline Vector4D& Vector4D::operator/=(float fl)
{
	Assert( fl != 0.0f );
	const float oofl{1.0f / fl};

	return *this *= oofl;
}

inline Vector4D Vector4D::operator/(Vector4D const& v) const
{
	Vector4D r{x, y, z, w};
	return r /= v;
}

inline Vector4D& Vector4D::operator/=(Vector4D const& v)
{ 
	Assert( v.x != 0.0f && v.y != 0.0f && v.z != 0.0f && v.w != 0.0f );

	DirectX::XMStoreFloat4
	(
		XmBase(),
		DirectX::XMVectorDivide
		(
			DirectX::XMLoadFloat4( XmBase() ),
			DirectX::XMLoadFloat4( v.XmBase() )
		)
	);

	Assert( IsValid() );
	return *this;
}

inline void XM_CALLCONV Vector4DAdd( Vector4D const& a, Vector4D const& b, Vector4D& c )
{
	Assert( a.IsValid() && b.IsValid() );

	c = a + b;
}

inline void XM_CALLCONV Vector4DSubtract( Vector4D const& a, Vector4D const& b, Vector4D& c )
{
	Assert( a.IsValid() && b.IsValid() );
	
	c = a - b;
}

inline void XM_CALLCONV Vector4DMultiply( Vector4D const& a, vec_t b, Vector4D& c )
{
	Assert( a.IsValid() && IsFinite(b) );

	c = a * b;
}

inline void XM_CALLCONV Vector4DMultiply( Vector4D const& a, Vector4D const& b, Vector4D& c )
{
	Assert( a.IsValid() && b.IsValid() );

	c = a * b;
}

inline void XM_CALLCONV Vector4DDivide( Vector4D const& a, vec_t b, Vector4D& c )
{
	Assert( a.IsValid() );
	Assert( b != 0.0f );

	const vec_t oob = 1.0f / b;

	c = a * oob;
}

inline void XM_CALLCONV Vector4DDivide( Vector4D const& a, Vector4D const& b, Vector4D& c )
{
	Assert( a.IsValid() );
	Assert( (b.x != 0.0f) && (b.y != 0.0f) && (b.z != 0.0f) && (b.w != 0.0f) );

	c = a / b;
}

inline void XM_CALLCONV Vector4DMA( Vector4D const& start, float s, Vector4D const& dir, Vector4D& result )
{
	Assert( start.IsValid() && IsFinite(s) && dir.IsValid() );

	DirectX::XMStoreFloat4
	(
		result.XmBase(),
		DirectX::XMVectorMultiplyAdd
		(
			DirectX::XMLoadFloat4( dir.XmBase() ),
			DirectX::XMVectorReplicate( s ), //-V2002
			DirectX::XMLoadFloat4( start.XmBase() )
		)
	);
}

// FIXME: Remove
// For backwards compatability
inline void	Vector4D::MulAdd(Vector4D const& a, Vector4D const& b, float scalar)
{
	x = a.x + b.x * scalar;
	y = a.y + b.y * scalar;
	z = a.z + b.z * scalar;
	w = a.w + b.w * scalar;
}

inline void XM_CALLCONV Vector4DLerp(const Vector4D& src1, const Vector4D& src2, vec_t t, Vector4D& dest )
{
	DirectX::XMStoreFloat4
	(
		dest.XmBase(),
		DirectX::XMVectorLerp
		(
			DirectX::XMLoadFloat4( src1.XmBase() ),
			DirectX::XMLoadFloat4( src2.XmBase() ),
			t
		)
	);
}

//-----------------------------------------------------------------------------
// dot, cross
//-----------------------------------------------------------------------------

inline vec_t XM_CALLCONV DotProduct4D(const Vector4D& a, const Vector4D& b) 
{ 
	Assert( a.IsValid() && b.IsValid() );

	return DirectX::XMVectorGetX //-V2002
	(
		DirectX::XMVector4Dot
		(
			DirectX::XMLoadFloat4( a.XmBase() ),
			DirectX::XMLoadFloat4( b.XmBase() )
		)
	);
}

// for backwards compatability
inline vec_t Vector4D::Dot( Vector4D const& vOther ) const
{
	return DotProduct4D( *this, vOther );
}


//-----------------------------------------------------------------------------
// length
//-----------------------------------------------------------------------------

inline vec_t XM_CALLCONV Vector4DLength( Vector4D const& v )
{
	Assert( v.IsValid() );
	return DirectX::XMVectorGetX( DirectX::XMVector4Length( DirectX::XMLoadFloat4( v.XmBase() ) ) ); //-V2002
}

inline vec_t Vector4D::LengthSqr() const
{ 
	Assert( IsValid() );
	return DirectX::XMVectorGetX( DirectX::XMVector4LengthSq( DirectX::XMLoadFloat4( XmBase() ) ) ); //-V2002
}

inline vec_t Vector4D::Length() const
{
	return Vector4DLength( *this );
}


//-----------------------------------------------------------------------------
// Normalization
//-----------------------------------------------------------------------------
inline vec_t XM_CALLCONV Vector4DNormalize( Vector4D& v )
{
	Assert( v.IsValid() );
	
	DirectX::XMVECTOR val = DirectX::XMLoadFloat4( v.XmBase() );
	DirectX::XMVECTOR len = DirectX::XMVector4Length( val );
	float slen = DirectX::XMVectorGetX( len ); //-V2002
	// Prevent division on zero.
	DirectX::XMVECTOR den = DirectX::XMVectorReplicate( 1.f / ( slen + FLT_EPSILON ) ); //-V2002

	DirectX::XMStoreFloat4( v.XmBase(), DirectX::XMVectorMultiply( val, den ) );
	
	return slen;
}

//-----------------------------------------------------------------------------
// Get the distance from this Vector4D to the other one 
//-----------------------------------------------------------------------------

inline vec_t Vector4D::DistTo(const Vector4D &vOther) const
{
	const Vector4D delta{*this - vOther};
	return delta.Length();
}

inline vec_t Vector4D::DistToSqr(const Vector4D &vOther) const
{
	const Vector4D delta{*this - vOther};
	return delta.LengthSqr();
}


//-----------------------------------------------------------------------------
// Vector4DAligned routines
//-----------------------------------------------------------------------------

inline Vector4DAligned::Vector4DAligned( vec_t X, vec_t Y, vec_t Z, vec_t W )
{
	x = X; y = Y; z = Z; w = W;
	Assert( IsValid() );
}

inline void Vector4DAligned::Set( vec_t X, vec_t Y, vec_t Z, vec_t W )
{
	x = X; y = Y; z = Z; w = W;
	Assert( IsValid() );
}

inline void Vector4DAligned::InitZero()
{
	x = 0.0f; y = 0.0f; z = 0.0f; w = 0.0f;
}

inline void XM_CALLCONV Vector4DMultiplyAligned( Vector4DAligned const& a, Vector4DAligned const& b, Vector4DAligned& c )
{
	Assert( a.IsValid() && b.IsValid() );

	DirectX::XMStoreFloat4A
	(
		c.XmBase(),
		DirectX::XMVectorMultiply
		(
			DirectX::XMLoadFloat4A( a.XmBase() ),
			DirectX::XMLoadFloat4A( b.XmBase() )
		)
	);
}

inline void XM_CALLCONV Vector4DWeightMAD( vec_t w, Vector4DAligned const& vInA, Vector4DAligned& vOutA, Vector4DAligned const& vInB, Vector4DAligned& vOutB )
{
	Assert( vInA.IsValid() && vInB.IsValid() && IsFinite(w) );

	DirectX::XMVECTOR vw = DirectX::XMVectorReplicate( w ); //-V2002

	DirectX::XMStoreFloat4A
	(
		vOutA.XmBase(),
		DirectX::XMVectorMultiplyAdd
		(
			DirectX::XMLoadFloat4A( vInA.XmBase() ),
			vw,
			DirectX::XMLoadFloat4A( vOutA.XmBase() )
		)
	);

	DirectX::XMStoreFloat4A
	(
		vOutB.XmBase(),
		DirectX::XMVectorMultiplyAdd
		(
			DirectX::XMLoadFloat4A( vInB.XmBase() ),
			vw,
			DirectX::XMLoadFloat4A( vOutB.XmBase() )
		)
	);
}

inline void XM_CALLCONV Vector4DWeightMADSSE( vec_t w, Vector4DAligned const& vInA, Vector4DAligned& vOutA, Vector4DAligned const& vInB, Vector4DAligned& vOutB )
{
	Vector4DWeightMAD( w, vInA, vOutA, vInB, vOutB );
}

#endif // VECTOR4D_H

