//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef VECTOR2D_H
#define VECTOR2D_H

#ifdef _WIN32
#pragma once
#endif

#include <DirectXMath.h>

#include <cmath>
#include <cstdlib>  // rand

#include "tier0/basetypes.h"  // vec_t
#include "tier0/dbg.h"
#include "mathlib/math_pfns.h"

//=========================================================
// 2D Vector2D
//=========================================================

// dimhotepus: alignas to use vec_t operator[] safely.
// dimhotepus: XM_CALLCONV to speedup calls.
class alignas(vec_t) Vector2D
{
public:
	// Members
	vec_t x, y;

	// Construction/destruction
#ifdef _DEBUG
    // Initialize to NAN to catch errors
    Vector2D() : x{VEC_T_NAN}, y{VEC_T_NAN}
	{
	}
#else
	Vector2D() = default;
#endif

	Vector2D(vec_t X, vec_t Y);
	Vector2D(const float *pFloat);

	// Initialization
	void XM_CALLCONV Init(vec_t ix=0.0f, vec_t iy=0.0f);

	// Got any nasty NAN's?
	bool XM_CALLCONV IsValid() const;

	// array access...
	vec_t XM_CALLCONV operator[](int i) const; //-V302
	vec_t& XM_CALLCONV operator[](int i); //-V302

	// Base address...
	vec_t* XM_CALLCONV Base();
	vec_t const* XM_CALLCONV Base() const;

	// dimhotepus: Better DirectX math integration.
	DirectX::XMFLOAT2* XM_CALLCONV XmBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT2) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT2) == alignof(Vector2D));
		return reinterpret_cast<DirectX::XMFLOAT2*>(this);
	}
	DirectX::XMFLOAT2 const* XM_CALLCONV XmBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT2) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT2) == alignof(Vector2D));
		return reinterpret_cast<DirectX::XMFLOAT2 const*>(this);
	}

	// Initialization methods
	void XM_CALLCONV Random( float minVal, float maxVal );

	// equality
	bool XM_CALLCONV operator==(const Vector2D& v) const;
	bool XM_CALLCONV operator!=(const Vector2D& v) const;

	// arithmetic operations
	Vector2D& XM_CALLCONV operator+=(const Vector2D &v);
	Vector2D& XM_CALLCONV operator-=(const Vector2D &v);
	Vector2D& XM_CALLCONV operator*=(const Vector2D &v);
	Vector2D& XM_CALLCONV operator*=(float s);
	Vector2D& XM_CALLCONV operator/=(const Vector2D &v);
	Vector2D& XM_CALLCONV operator/=(float s);

	// negate the Vector2D components
	void	Negate();

	// Get the Vector2D's magnitude.
	vec_t XM_CALLCONV Length() const;

	// Get the Vector2D's magnitude squared.
	vec_t XM_CALLCONV LengthSqr() const;

	// return true if this vector is (0,0) within tolerance
	bool XM_CALLCONV IsZero( float tolerance = 0.01f ) const
	{
		return (x > -tolerance && x < tolerance &&
				y > -tolerance && y < tolerance);
	}

	// Normalize in place and return the old length.
	vec_t XM_CALLCONV NormalizeInPlace();

	// Compare length.
	bool XM_CALLCONV IsLengthGreaterThan( float val ) const;
	bool XM_CALLCONV IsLengthLessThan( float val ) const;

	// Get the distance from this Vector2D to the other one.
	vec_t XM_CALLCONV DistTo(const Vector2D &vOther) const;

	// Get the distance from this Vector2D to the other one squared.
	vec_t XM_CALLCONV DistToSqr(const Vector2D &vOther) const;

	// Copy
	void XM_CALLCONV CopyToArray(float* rgfl) const;

	// Multiply, add, and assign to this (ie: *this = a + b * scalar). This
	// is about 12% faster than the actual Vector2D equation (because it's done per-component
	// rather than per-Vector2D).
	[[deprecated("Use Vector2DMA")]] void XM_CALLCONV MulAdd(const Vector2D& a, const Vector2D& b, float scalar);

	// Dot product.
	vec_t XM_CALLCONV Dot(const Vector2D& vOther) const;

	// assignment
	Vector2D& XM_CALLCONV operator=(const Vector2D &vOther);

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// copy constructors
	Vector2D(const Vector2D &vOther);

	// arithmetic operations
	Vector2D XM_CALLCONV operator-() const;
				
	Vector2D XM_CALLCONV operator+(const Vector2D& v) const;
	Vector2D XM_CALLCONV operator-(const Vector2D& v) const;
	Vector2D XM_CALLCONV operator*(const Vector2D& v) const;
	Vector2D XM_CALLCONV operator/(const Vector2D& v) const;
	Vector2D XM_CALLCONV operator*(float fl) const;
	Vector2D XM_CALLCONV operator/(float fl) const;

	// Returns a Vector2D with the min or max in X, Y, and Z.
	Vector2D XM_CALLCONV Min(const Vector2D &vOther) const;
	Vector2D XM_CALLCONV Max(const Vector2D &vOther) const;

#else

private:
	// No copy constructors allowed if we're in optimal mode
	Vector2D(const Vector2D& vOther);
#endif
};

//-----------------------------------------------------------------------------

// dimhotepus: inline
const inline Vector2D vec2_origin( 0, 0 );
// dimhotepus: inline
const inline Vector2D vec2_invalid( FLT_MAX, FLT_MAX );

//-----------------------------------------------------------------------------
// Vector2D related operations
//-----------------------------------------------------------------------------

// Vector2D clear
void XM_CALLCONV Vector2DClear( Vector2D& a );

// Copy
void XM_CALLCONV Vector2DCopy( const Vector2D& src, Vector2D& dst );

// Vector2D arithmetic
void XM_CALLCONV Vector2DAdd( const Vector2D& a, const Vector2D& b, Vector2D& result );
void XM_CALLCONV Vector2DSubtract( const Vector2D& a, const Vector2D& b, Vector2D& result );
void XM_CALLCONV Vector2DMultiply( const Vector2D& a, vec_t b, Vector2D& result );
void XM_CALLCONV Vector2DMultiply( const Vector2D& a, const Vector2D& b, Vector2D& result );
void XM_CALLCONV Vector2DDivide( const Vector2D& a, vec_t b, Vector2D& result );
void XM_CALLCONV Vector2DDivide( const Vector2D& a, const Vector2D& b, Vector2D& result );
void XM_CALLCONV Vector2DMA( const Vector2D& start, float s, const Vector2D& dir, Vector2D& result );

// Store the min or max of each of x, y, and z into the result.
void XM_CALLCONV Vector2DMin( const Vector2D &a, const Vector2D &b, Vector2D &result );
void XM_CALLCONV Vector2DMax( const Vector2D &a, const Vector2D &b, Vector2D &result );

#define Vector2DExpand( v ) (v).x, (v).y

// Normalization
vec_t XM_CALLCONV Vector2DNormalize( Vector2D& v );

// Length
vec_t XM_CALLCONV Vector2DLength( const Vector2D& v );

// Dot Product
vec_t XM_CALLCONV DotProduct2D( const Vector2D& a, const Vector2D& b );

// Linearly interpolate between two vectors
void XM_CALLCONV Vector2DLerp(const Vector2D& src1, const Vector2D& src2, vec_t t, Vector2D& dest );


//-----------------------------------------------------------------------------
//
// Inlined Vector2D methods
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// constructors
//-----------------------------------------------------------------------------
inline Vector2D::Vector2D(vec_t X, vec_t Y)
	: x{X}, y{Y}
{ 
	Assert( IsValid() );
}

inline Vector2D::Vector2D(const float *pFloat)
{
	Assert( pFloat );
	x = pFloat[0]; y = pFloat[1];
	Assert( IsValid() );
}


//-----------------------------------------------------------------------------
// copy constructor
//-----------------------------------------------------------------------------

inline Vector2D::Vector2D(const Vector2D &vOther)
	: x{vOther.x}, y{vOther.y}
{ 
	Assert( vOther.IsValid() );
}

//-----------------------------------------------------------------------------
// initialization
//-----------------------------------------------------------------------------

inline void Vector2D::Init( vec_t ix, vec_t iy )
{ 
	x = ix; y = iy;
	Assert( IsValid() );
}

inline void Vector2D::Random( float minVal, float maxVal )
{
	DirectX::XMVECTOR rn = DirectX::XMVectorSet
	(
		(static_cast<float>(rand()) / VALVE_RAND_MAX),
		(static_cast<float>(rand()) / VALVE_RAND_MAX),
		0.0f,
		0.0f
	);
	DirectX::XMVECTOR mn = DirectX::XMVectorReplicate( minVal ); //-V2002

	DirectX::XMStoreFloat2
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

inline void XM_CALLCONV Vector2DClear( Vector2D& a )
{
	a.x = a.y = 0.0f;
}

//-----------------------------------------------------------------------------
// assignment
//-----------------------------------------------------------------------------

inline Vector2D& Vector2D::operator=(const Vector2D &vOther)
{
	Assert( vOther.IsValid() );
	x=vOther.x; y=vOther.y;
	return *this; 
}

//-----------------------------------------------------------------------------
// Array access
//-----------------------------------------------------------------------------

inline vec_t& Vector2D::operator[](int i)
{
	Assert( (i >= 0) && (i < 2) );
	static_assert(alignof(Vector2D) == alignof(vec_t));
	return reinterpret_cast<vec_t*>(this)[i]; //-V108
}

inline vec_t Vector2D::operator[](int i) const
{
	Assert( (i >= 0) && (i < 2) );
	static_assert(alignof(Vector2D) == alignof(vec_t));
	return reinterpret_cast<const vec_t*>(this)[i]; //-V108
}

//-----------------------------------------------------------------------------
// Base address...
//-----------------------------------------------------------------------------

inline vec_t* Vector2D::Base()
{
	static_assert(alignof(Vector2D) == alignof(vec_t));
	return reinterpret_cast<vec_t*>(this);
}

inline vec_t const* Vector2D::Base() const
{
	static_assert(alignof(Vector2D) == alignof(vec_t));
	return reinterpret_cast<const vec_t*>(this);
}

//-----------------------------------------------------------------------------
// IsValid?
//-----------------------------------------------------------------------------

inline bool Vector2D::IsValid() const
{
	return !DirectX::XMVector2IsNaN( DirectX::XMLoadFloat2( XmBase() ) );
}

//-----------------------------------------------------------------------------
// comparison
//-----------------------------------------------------------------------------

inline bool Vector2D::operator==( const Vector2D& src ) const
{
	Assert( src.IsValid() && IsValid() );
	return x == src.x && y == src.y;
}

inline bool Vector2D::operator!=( const Vector2D& src ) const
{
	return !(*this == src);
}


//-----------------------------------------------------------------------------
// Copy
//-----------------------------------------------------------------------------

inline void XM_CALLCONV Vector2DCopy( const Vector2D& src, Vector2D& dst )
{
	Assert( src.IsValid() );
	dst.x = src.x;
	dst.y = src.y;
}

inline void	XM_CALLCONV Vector2D::CopyToArray(float* rgfl) const
{
	Assert( IsValid() );
	Assert( rgfl );
	rgfl[0] = x; rgfl[1] = y;
}

//-----------------------------------------------------------------------------
// standard math operations
//-----------------------------------------------------------------------------

inline void Vector2D::Negate()
{ 
	Assert( IsValid() );
	x = -x;
	y = -y;
} 

inline Vector2D& Vector2D::operator+=(const Vector2D& v)
{ 
	Assert( IsValid() && v.IsValid() );
	x += v.x;
	y += v.y;
	return *this;
}

inline Vector2D& Vector2D::operator-=(const Vector2D& v)
{ 
	Assert( IsValid() && v.IsValid() );
	x -= v.x;
	y -= v.y;
	return *this;
}

inline Vector2D& Vector2D::operator*=(float fl)
{
	x *= fl;
	y *= fl;
	Assert( IsValid() );
	return *this;
}

inline Vector2D& Vector2D::operator*=(const Vector2D& v)
{
	x *= v.x;
	y *= v.y;
	Assert( IsValid() );
	return *this;
}

inline Vector2D& Vector2D::operator/=(float fl)
{
	Assert( fl != 0.0f );
	
	const float oob = 1.0f / fl;

	x *= oob;
	y *= oob;

	Assert( IsValid() );
	return *this;
}

inline Vector2D& Vector2D::operator/=(const Vector2D& v)
{ 
	Assert( v.x != 0.0f && v.y != 0.0f );
	x /= v.x;
	y /= v.y;
	Assert( IsValid() );
	return *this;
}

inline void XM_CALLCONV Vector2DAdd( const Vector2D& a, const Vector2D& b, Vector2D& c )
{
	Assert( a.IsValid() && b.IsValid() );

	c = a + b;
}

inline void XM_CALLCONV Vector2DSubtract( const Vector2D& a, const Vector2D& b, Vector2D& c )
{
	Assert( a.IsValid() && b.IsValid() );
	
	c = a - b;
}

inline void XM_CALLCONV Vector2DMultiply( const Vector2D& a, vec_t b, Vector2D& c )
{
	Assert( a.IsValid() && IsFinite(b) );

	c = a * b;
}

inline void XM_CALLCONV Vector2DMultiply( const Vector2D& a, const Vector2D& b, Vector2D& c )
{
	Assert( a.IsValid() && b.IsValid() );

	c = a * b;
}


inline void XM_CALLCONV Vector2DDivide( const Vector2D& a, vec_t b, Vector2D& c )
{
	Assert( b != 0.0f );
	const vec_t oob{1.0f / b};

	c = a * oob;
}

inline void XM_CALLCONV Vector2DDivide( const Vector2D& a, const Vector2D& b, Vector2D& c )
{
	Assert( a.IsValid() );
	Assert( (b.x != 0.0f) && (b.y != 0.0f) );

	DirectX::XMStoreFloat2
	(
		c.XmBase(),
		DirectX::XMVectorDivide
		(
			DirectX::XMLoadFloat2( a.XmBase() ),
			// Ensure no zero division on W & Z.
			DirectX::XMVectorSetW //-V2002
			(
				DirectX::XMVectorSetZ //-V2002
				(
					DirectX::XMLoadFloat2( b.XmBase() ), 1.0f
				),
				1.0f
			)
		)
	);
}

inline void XM_CALLCONV Vector2DMA( const Vector2D& start, float s, const Vector2D& dir, Vector2D& result )
{
	Assert( start.IsValid() && IsFinite(s) && dir.IsValid() );

	DirectX::XMStoreFloat2
	(
		result.XmBase(),
		DirectX::XMVectorMultiplyAdd
		(
			DirectX::XMLoadFloat2( dir.XmBase() ),
			DirectX::XMVectorReplicate( s ), //-V2002
			DirectX::XMLoadFloat2( start.XmBase() )
		)
	);
}

// FIXME: Remove
// For backwards compatability
inline void	XM_CALLCONV Vector2D::MulAdd(const Vector2D& a, const Vector2D& b, float scalar)
{
	Vector2DMA( a, scalar, b, *this );
}

inline void XM_CALLCONV Vector2DLerp(const Vector2D& src1, const Vector2D& src2, vec_t t, Vector2D& dest )
{
	DirectX::XMStoreFloat2
	(
		dest.XmBase(),
		DirectX::XMVectorLerp
		(
			DirectX::XMLoadFloat2( src1.XmBase() ),
			DirectX::XMLoadFloat2( src2.XmBase() ),
			t
		)
	);
}

//-----------------------------------------------------------------------------
// dot, cross
//-----------------------------------------------------------------------------
inline vec_t XM_CALLCONV DotProduct2D(const Vector2D& a, const Vector2D& b) 
{ 
	Assert( a.IsValid() && b.IsValid() );

	return DirectX::XMVectorGetX //-V2002
	(
		DirectX::XMVector2Dot
		(
			DirectX::XMLoadFloat2( a.XmBase() ),
			DirectX::XMLoadFloat2( b.XmBase() )
		)
	);
}

// for backwards compatability
inline vec_t Vector2D::Dot( const Vector2D& vOther ) const
{
	return DotProduct2D( *this, vOther );
}


//-----------------------------------------------------------------------------
// length
//-----------------------------------------------------------------------------
inline vec_t XM_CALLCONV Vector2DLength( const Vector2D& v )
{
	Assert( v.IsValid() );
	return FastSqrt( DotProduct2D( v, v ) );
}

inline vec_t XM_CALLCONV Vector2D::LengthSqr() const
{ 
	Assert( IsValid() );
	return DotProduct2D( *this, *this );
}

inline vec_t Vector2D::NormalizeInPlace()
{
	return Vector2DNormalize( *this );
}

inline bool Vector2D::IsLengthGreaterThan( float val ) const
{
	return LengthSqr() > val*val;
}

inline bool Vector2D::IsLengthLessThan( float val ) const
{
	return LengthSqr() < val*val;
}

inline vec_t Vector2D::Length() const
{
	return Vector2DLength( *this );
}


inline void XM_CALLCONV Vector2DMin( const Vector2D &a, const Vector2D &b, Vector2D &result )
{
	result.x = min( a.x, b.x );
	result.y = min( a.y, b.y );
}


inline void XM_CALLCONV Vector2DMax( const Vector2D &a, const Vector2D &b, Vector2D &result )
{
	result.x = max( a.x, b.x );
	result.y = max( a.y, b.y );
}


//-----------------------------------------------------------------------------
// Normalization
//-----------------------------------------------------------------------------
inline vec_t XM_CALLCONV Vector2DNormalize( Vector2D& v )
{
	Assert( v.IsValid() );
	
	DirectX::XMVECTOR val = DirectX::XMLoadFloat2( v.XmBase() );
	DirectX::XMVECTOR len = DirectX::XMVector2Length( val );
	float slen = DirectX::XMVectorGetX( len ); //-V2002
	// Prevent division on zero.
	DirectX::XMVECTOR den = DirectX::XMVectorReplicate( 1.f / ( slen + FLT_EPSILON ) ); //-V2002

	DirectX::XMStoreFloat2( v.XmBase(), DirectX::XMVectorMultiply( val, den ) );
	
	return slen;
}


//-----------------------------------------------------------------------------
// Get the distance from this Vector2D to the other one 
//-----------------------------------------------------------------------------
inline vec_t Vector2D::DistTo(const Vector2D &vOther) const
{
	const Vector2D delta{*this - vOther};
	return delta.Length();
}

inline vec_t Vector2D::DistToSqr(const Vector2D &vOther) const
{
	const Vector2D delta{*this - vOther};
	return delta.LengthSqr();
}


//-----------------------------------------------------------------------------
// Computes the closest point to vecTarget no farther than flMaxDist from vecStart
//-----------------------------------------------------------------------------
inline void ComputeClosestPoint2D( const Vector2D& vecStart, float flMaxDist, const Vector2D& vecTarget, Vector2D *pResult )
{
	Vector2D vecDelta{vecTarget - vecStart};
	float flDistSqr = vecDelta.LengthSqr();
	if ( flDistSqr <= flMaxDist * flMaxDist )
	{
		*pResult = vecTarget;
	}
	else
	{
		vecDelta /= FastSqrt( flDistSqr );
		Vector2DMA( vecStart, flMaxDist, vecDelta, *pResult );
	}
}



//-----------------------------------------------------------------------------
//
// Slow methods
//
//-----------------------------------------------------------------------------

#ifndef VECTOR_NO_SLOW_OPERATIONS

//-----------------------------------------------------------------------------
// Returns a Vector2D with the min or max in X, Y, and Z.
//-----------------------------------------------------------------------------

inline Vector2D Vector2D::Min(const Vector2D &vOther) const
{
	return {min(x, vOther.x), min(y, vOther.y)};
}

inline Vector2D Vector2D::Max(const Vector2D &vOther) const
{
	return {max(x, vOther.x), max(y, vOther.y)};
}


//-----------------------------------------------------------------------------
// arithmetic operations
//-----------------------------------------------------------------------------

inline Vector2D Vector2D::operator-() const
{ 
	return {-x, -y};
}

inline Vector2D Vector2D::operator+(const Vector2D& v) const
{ 
	return {x + v.x, y + v.y};
}

inline Vector2D Vector2D::operator-(const Vector2D& v) const
{ 
	return {x - v.x, y - v.y};
}

inline Vector2D Vector2D::operator*(float fl) const
{ 
	return {x * fl, y * fl};
}

inline Vector2D Vector2D::operator*(const Vector2D& v) const
{ 
	return {x * v.x, y * v.y};
}

inline Vector2D Vector2D::operator/(float fl) const
{ 
	const float oob{1.0f / fl};
	return {x * oob, y * oob};
}

inline Vector2D Vector2D::operator/(const Vector2D& v) const
{ 
	return {x / v.x, y / v.y};
}

inline Vector2D operator*(float fl, const Vector2D& v)
{ 
	return v * fl;
}

#endif //slow

#endif // VECTOR2D_H

