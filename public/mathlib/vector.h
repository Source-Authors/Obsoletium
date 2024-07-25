//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef VECTOR_H
#define VECTOR_H

#ifdef _WIN32
#pragma once
#endif

#include <DirectXMath.h>

#include <cfloat>
#include <cstdlib>  // rand

#include "tier0/basetypes.h"  // vec_t
#include "tier0/dbg.h"
#include "tier0/threadtools.h"

#include "mathlib/vector2d.h"
#include "mathlib/math_pfns.h"

// Uncomment this to add extra Asserts to check for NANs, uninitialized vecs, etc.
//#define VECTOR_PARANOIA	1

// Uncomment this to make sure we don't do anything slow with our vectors
//#define VECTOR_NO_SLOW_OPERATIONS 1


// Used to make certain code easier to read.
#define X_INDEX	0
#define Y_INDEX	1
#define Z_INDEX	2


#ifdef VECTOR_PARANOIA
#define CHECK_VALID( _v)	Assert( (_v).IsValid() )
#else
#ifdef GNUC
#define CHECK_VALID( _v)
#else
#define CHECK_VALID( _v)	0
#endif
#endif

#define VecToString(v)	(static_cast<const char *>(CFmtStr("(%f, %f, %f)", (v).x, (v).y, (v).z))) // ** Note: this generates a temporary, don't hold reference!

class VectorByValue;

//=========================================================
// 3D Vector
//=========================================================

// dimhotepus: alignas to use vec_t operator[] safely.
// dimhotepus: XM_CALLCONV to speedup calls.
class alignas(vec_t) Vector
{
public:
	// Members
	vec_t x, y, z;

	// Construction/destruction: 
#if defined(_DEBUG) && defined(VECTOR_PARANOIA)
    // Initialize to NAN to catch errors
	Vector() : x(VEC_T_NAN), y(VEC_T_NAN), z(VEC_T_NAN)
	{
	}
#else
	Vector() = default;
#endif
	Vector(vec_t X, vec_t Y, vec_t Z);
	explicit Vector(vec_t XYZ); ///< broadcast initialize

	// Initialization
	void Init(vec_t ix=0.0f, vec_t iy=0.0f, vec_t iz=0.0f);
	 // TODO (Ilya): Should there be an init that takes a single float for consistency?

	// Got any nasty NAN's?
	bool XM_CALLCONV IsValid() const;
	void Invalidate();

	// array access...
	vec_t XM_CALLCONV operator[](int i) const;
	vec_t& XM_CALLCONV operator[](int i);

	// Base address...
	vec_t* XM_CALLCONV Base();
	vec_t const* XM_CALLCONV Base() const;

	// dimhotepus: Better DirectX math integration.
	DirectX::XMFLOAT3* XM_CALLCONV XmBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT3) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT3) == alignof(Vector));
		return reinterpret_cast<DirectX::XMFLOAT3*>(this);
	}
	DirectX::XMFLOAT3 const* XM_CALLCONV XmBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT3) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT3) == alignof(Vector));
		return reinterpret_cast<DirectX::XMFLOAT3 const*>(this);
	}

	// Cast to Vector2D...
	Vector2D& XM_CALLCONV AsVector2D();
	const Vector2D& XM_CALLCONV AsVector2D() const;

	// Initialization methods
	void XM_CALLCONV Random( vec_t minVal, vec_t maxVal );
	inline void Zero(); ///< zero out a vector

	// equality
	bool XM_CALLCONV operator==(const Vector& v) const;
	bool XM_CALLCONV operator!=(const Vector& v) const;

	// arithmetic operations
	FORCEINLINE Vector&	XM_CALLCONV operator+=(const Vector &v);
	FORCEINLINE Vector&	XM_CALLCONV operator-=(const Vector &v);
	FORCEINLINE Vector&	XM_CALLCONV operator*=(const Vector &v);
	FORCEINLINE Vector&	XM_CALLCONV operator*=(float s);
	FORCEINLINE Vector&	XM_CALLCONV operator/=(const Vector &v);
	FORCEINLINE Vector&	XM_CALLCONV operator/=(float s);
	FORCEINLINE Vector&	XM_CALLCONV operator+=(float fl) ; ///< broadcast add
	FORCEINLINE Vector&	XM_CALLCONV operator-=(float fl) ; ///< broadcast sub

// negate the vector components
	void	Negate(); 

	// Get the vector's magnitude.
	inline vec_t XM_CALLCONV Length() const;

	// Get the vector's magnitude squared.
	FORCEINLINE vec_t XM_CALLCONV LengthSqr() const
	{ 
		CHECK_VALID(*this);
		return DirectX::XMVectorGetX( DirectX::XMVector3LengthSq( DirectX::XMLoadFloat3( XmBase() ) ) );
	}

	// return true if this vector is (0,0,0) within tolerance
	bool XM_CALLCONV IsZero( float tolerance = 0.01f ) const
	{
		return DirectX::XMVector3NearEqual
		(
			DirectX::g_XMZero,
			DirectX::XMLoadFloat3( XmBase() ),
			DirectX::XMVectorReplicate( tolerance )
		);
	}

	vec_t XM_CALLCONV NormalizeInPlace();
	Vector XM_CALLCONV Normalized() const;
	bool XM_CALLCONV IsLengthGreaterThan( float val ) const;
	bool XM_CALLCONV IsLengthLessThan( float val ) const;

	// check if a vector is within the box defined by two other vectors
	FORCEINLINE bool XM_CALLCONV WithinAABox( Vector const &boxmin, Vector const &boxmax) const;
 
	// Get the distance from this vector to the other one.
	vec_t XM_CALLCONV DistTo(const Vector &vOther) const;

	// Get the distance from this vector to the other one squared.
	FORCEINLINE vec_t XM_CALLCONV DistToSqr(const Vector &vOther) const
	{
		DirectX::XMVECTOR self = DirectX::XMLoadFloat3( XmBase() );
		DirectX::XMVECTOR other = DirectX::XMLoadFloat3( vOther.XmBase() );

		return DirectX::XMVectorGetX
		(
			DirectX::XMVector3LengthSq( DirectX::XMVectorSubtract( self, other ) )
		);
	}

	// Copy
	void XM_CALLCONV CopyToArray(float* rgfl) const;

	// Multiply, add, and assign to this (ie: *this = a + b * scalar). This
	// is about 12% faster than the actual vector equation (because it's done per-component
	// rather than per-vector).
	[[deprecated("Use VectorMA")]] void	MulAdd(const Vector& a, const Vector& b, float scalar);	

	// Dot product.
	vec_t XM_CALLCONV Dot(const Vector& vOther) const;

	// 2d
	vec_t XM_CALLCONV Length2D() const;
	vec_t XM_CALLCONV Length2DSqr() const;

	XM_CALLCONV operator VectorByValue&()
	{
		//static_assert(alignof(VectorByValue) == alignof(Vector));
		return *reinterpret_cast<VectorByValue*>(this);
	}
	XM_CALLCONV operator const VectorByValue& () const
	{
		//static_assert(alignof(VectorByValue) == alignof(Vector));
		return *reinterpret_cast<const VectorByValue*>(this);
	}

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// copy constructors
//	Vector(const Vector &vOther);

	// arithmetic operations
	Vector XM_CALLCONV operator-() const;

	Vector XM_CALLCONV operator+(const Vector& v) const;
	Vector XM_CALLCONV operator-(const Vector& v) const;
	Vector XM_CALLCONV operator*(const Vector& v) const;
	Vector XM_CALLCONV operator/(const Vector& v) const;
	Vector XM_CALLCONV operator*(float fl) const;
	Vector XM_CALLCONV operator/(float fl) const;
	
	// Cross product between two vectors.
	Vector XM_CALLCONV Cross(const Vector &vOther) const;

	// Returns a vector with the min or max in X, Y, and Z.
	Vector XM_CALLCONV Min(const Vector &vOther) const;
	Vector XM_CALLCONV Max(const Vector &vOther) const;

#else

private:
	// No copy constructors allowed if we're in optimal mode
	Vector(const Vector& vOther);
#endif
};

FORCEINLINE void XM_CALLCONV NetworkVarConstruct( Vector &v ) { v.Zero(); }


#define USE_M64S ( ( !defined( _X360 ) ) )



//=========================================================
// 4D Short Vector (aligned on 8-byte boundary)
//=========================================================
class ALIGN8 ShortVector : CAlignedNewDelete<8>
{
public:
	short x, y, z, w;

	// Initialization
	void XM_CALLCONV Init(short ix = 0, short iy = 0, short iz = 0, short iw = 0 );


#ifdef USE_M64S
	__m64 &AsM64() { return *(__m64*)&x; }
	const __m64 &AsM64() const { return *(const __m64*)&x; } 
#endif

	// Setter
	void XM_CALLCONV Set( const ShortVector& vOther );
	void XM_CALLCONV Set( const short ix, const short iy, const short iz, const short iw );

	// array access...
	short XM_CALLCONV operator[](int i) const;
	short& XM_CALLCONV operator[](int i);

	// Base address...
	short* XM_CALLCONV Base();
	short const* XM_CALLCONV Base() const;

	// equality
	bool XM_CALLCONV operator==(const ShortVector& v) const;
	bool XM_CALLCONV operator!=(const ShortVector& v) const;

	// Arithmetic operations
	FORCEINLINE ShortVector& XM_CALLCONV operator+=(const ShortVector &v);
	FORCEINLINE ShortVector& XM_CALLCONV operator-=(const ShortVector &v);
	FORCEINLINE ShortVector& XM_CALLCONV operator*=(const ShortVector &v);
	FORCEINLINE ShortVector& XM_CALLCONV operator*=(float s);
	FORCEINLINE ShortVector& XM_CALLCONV operator/=(const ShortVector &v);
	FORCEINLINE ShortVector& XM_CALLCONV operator/=(float s);
	FORCEINLINE ShortVector XM_CALLCONV operator*(float fl) const;

private:

	// No copy constructors allowed if we're in optimal mode
//	ShortVector(ShortVector const& vOther);

	// No assignment operators either...
//	ShortVector& operator=( ShortVector const& src );

} ALIGN8_POST;






//=========================================================
// 4D Integer Vector
//=========================================================

// dimhotepus: alignas to use int operator[] safely.
// dimhotepus: XM_CALLCONV to speedup calls.
class alignas(int) IntVector4D
{
public:
	int x, y, z, w;

	// Initialization
	void XM_CALLCONV Init(int ix = 0, int iy = 0, int iz = 0, int iw = 0 );

#ifdef USE_M64S
	// dimhotepus: Dropped as obsolete and x86 specific.
	// [[deprecated("Use SSE2 stuff")]] __m64 &AsM64() { return *(__m64*)&x; }
	// [[deprecated("Use SSE2 stuff")]] const __m64 &AsM64() const { return *(const __m64*)&x; } 
#endif

	// Setter
	void XM_CALLCONV Set( const IntVector4D& vOther );
	void XM_CALLCONV Set( const int ix, const int iy, const int iz, const int iw );

	// array access...
	int XM_CALLCONV operator[](int i) const;
	int& XM_CALLCONV operator[](int i);

	// Base address...
	int* XM_CALLCONV Base();
	int const* XM_CALLCONV Base() const;

	// dimhotepus: Better DirectX math integration.
	DirectX::XMINT4* XM_CALLCONV XmBase()
	{
		static_assert(sizeof(DirectX::XMINT4) == sizeof(*this));
		static_assert(alignof(DirectX::XMINT4) == alignof(IntVector4D));
		return reinterpret_cast<DirectX::XMINT4*>(this);
	}
	DirectX::XMINT4 const* XM_CALLCONV XmBase() const
	{
		static_assert(sizeof(DirectX::XMINT4) == sizeof(*this));
		static_assert(alignof(DirectX::XMINT4) == alignof(IntVector4D));
		return reinterpret_cast<DirectX::XMINT4 const*>(this);
	}

	// equality
	bool XM_CALLCONV operator==(const IntVector4D& v) const;
	bool XM_CALLCONV operator!=(const IntVector4D& v) const;

	// Arithmetic operations
	FORCEINLINE IntVector4D& XM_CALLCONV operator+=(const IntVector4D &v);
	FORCEINLINE IntVector4D& XM_CALLCONV operator-=(const IntVector4D &v);
	FORCEINLINE IntVector4D& XM_CALLCONV operator*=(const IntVector4D &v);
	FORCEINLINE IntVector4D& XM_CALLCONV operator*=(float s);
	FORCEINLINE IntVector4D& XM_CALLCONV operator/=(const IntVector4D &v);
	FORCEINLINE IntVector4D& XM_CALLCONV operator/=(float s);
	FORCEINLINE IntVector4D XM_CALLCONV operator*(float fl) const;

private:

	// No copy constructors allowed if we're in optimal mode
	//	IntVector4D(IntVector4D const& vOther);

	// No assignment operators either...
	//	IntVector4D& operator=( IntVector4D const& src );

};



//-----------------------------------------------------------------------------
// Allows us to specifically pass the vector by value when we need to
//-----------------------------------------------------------------------------
class VectorByValue : public Vector
{
public:
	// Construction/destruction:
	VectorByValue(void) : Vector() {} 
	VectorByValue(vec_t X, vec_t Y, vec_t Z) : Vector( X, Y, Z ) {}
	VectorByValue(const VectorByValue& vOther) = default;
};

static_assert(alignof(VectorByValue) == alignof(Vector),
	"Need for Vector::operator VectorByValue& {const} support.");

//-----------------------------------------------------------------------------
// Utility to simplify table construction. No constructor means can use
// traditional C-style initialization
//-----------------------------------------------------------------------------

// dimhotepus: alignas to use vec_t operator[] safely.
// dimhotepus: XM_CALLCONV to speedup calls.
class alignas(vec_t) TableVector
{
public:
	vec_t x, y, z;

	XM_CALLCONV operator Vector &()
	{
		static_assert(alignof(TableVector) == alignof(Vector));
		return *reinterpret_cast<Vector *>(this);
	}
	XM_CALLCONV operator const Vector &() const
	{
		static_assert(alignof(TableVector) == alignof(Vector));
		return *reinterpret_cast<const Vector *>(this);
	}

	// array access...
	inline vec_t& XM_CALLCONV operator[](int i)
	{
		Assert( (i >= 0) && (i < 3) );
		static_assert(alignof(TableVector) == alignof(vec_t));
		return reinterpret_cast<vec_t*>(this)[i];
	}

	inline vec_t XM_CALLCONV operator[](int i) const
	{
		Assert( (i >= 0) && (i < 3) );
		static_assert(alignof(TableVector) == alignof(vec_t));
		return reinterpret_cast<const vec_t*>(this)[i];
	}

	// dimhotepus: Better DirectX math integration.
	DirectX::XMFLOAT3* XM_CALLCONV XmBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT3) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT3) == alignof(TableVector));
		return reinterpret_cast<DirectX::XMFLOAT3*>(this);
	}
	DirectX::XMFLOAT3 const* XM_CALLCONV XmBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT3) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT3) == alignof(TableVector));
		return reinterpret_cast<DirectX::XMFLOAT3 const*>(this);
	}
};


//-----------------------------------------------------------------------------
// Here's where we add all those lovely SSE optimized routines
//-----------------------------------------------------------------------------
// dimhotepus: Fix aligned alloc.
class alignas(16) VectorAligned : public CAlignedNewDelete<16, Vector>
{
public:
	inline VectorAligned() = default;
	inline VectorAligned(vec_t X, vec_t Y, vec_t Z)  //-V730
	{
		Init(X,Y,Z);
		w = 0;
	}

#ifdef VECTOR_NO_SLOW_OPERATIONS

private:
	// No copy constructors allowed if we're in optimal mode
	VectorAligned(const VectorAligned& vOther);
	VectorAligned(const Vector &vOther);

#else
public:
	explicit VectorAligned(const Vector &vOther)  //-V730
	{
		Init(vOther.x, vOther.y, vOther.z);
		w = 0;
	}
	
	VectorAligned& XM_CALLCONV operator=(const Vector &vOther)
	{
		Init(vOther.x, vOther.y, vOther.z);
		return *this;
	}

	// dimhotepus: Better DirectX math integration.
	DirectX::XMFLOAT4A* XM_CALLCONV XmBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT4A) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT4A) == alignof(VectorAligned));
		return reinterpret_cast<DirectX::XMFLOAT4A*>(this);
	}
	DirectX::XMFLOAT4A const* XM_CALLCONV XmBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT4A) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT4A) == alignof(VectorAligned));
		return reinterpret_cast<DirectX::XMFLOAT4A const*>(this);
	}
	
#endif
	float w;	// this space is used anyway
};

//-----------------------------------------------------------------------------
// Vector related operations
//-----------------------------------------------------------------------------

// Vector clear
FORCEINLINE void XM_CALLCONV VectorClear( Vector& a );

// Copy
FORCEINLINE void XM_CALLCONV VectorCopy( const Vector& src, Vector& dst );

// Vector arithmetic
FORCEINLINE void XM_CALLCONV VectorAdd( const Vector& a, const Vector& b, Vector& result );
FORCEINLINE void XM_CALLCONV VectorSubtract( const Vector& a, const Vector& b, Vector& result );
FORCEINLINE void XM_CALLCONV VectorMultiply( const Vector& a, vec_t b, Vector& result );
FORCEINLINE void XM_CALLCONV VectorMultiply( const Vector& a, const Vector& b, Vector& result );
FORCEINLINE void XM_CALLCONV VectorDivide( const Vector& a, vec_t b, Vector& result );
FORCEINLINE void XM_CALLCONV VectorDivide( const Vector& a, const Vector& b, Vector& result );
inline void XM_CALLCONV VectorScale ( const Vector& in, vec_t scale, Vector& result );
// Don't mark this as inline in its function declaration. That's only necessary on its
// definition, and 'inline' here leads to gcc warnings.
void XM_CALLCONV VectorMA( const Vector& start, float scale, const Vector& direction, Vector& dest );

// Vector equality with tolerance
bool XM_CALLCONV VectorsAreEqual( const Vector& src1, const Vector& src2, float tolerance = 0.0f );

#define VectorExpand(v) (v).x, (v).y, (v).z


// Normalization
// FIXME: Can't use quite yet
//vec_t VectorNormalize( Vector& v );

// Length
inline vec_t XM_CALLCONV VectorLength( const Vector& v );

// Dot Product
FORCEINLINE vec_t XM_CALLCONV DotProduct(const Vector& a, const Vector& b);

// Cross product
void XM_CALLCONV CrossProduct(const Vector& a, const Vector& b, Vector& result );
void XM_CALLCONV CrossProduct(const Vector& a, const Vector& b, DirectX::XMFLOAT4 *result );
void XM_CALLCONV CrossProduct(const DirectX::XMFLOAT4 *a, const Vector& b, DirectX::XMFLOAT4 *result );
void XM_CALLCONV CrossProduct(const DirectX::XMFLOAT4 *a, const Vector& b, Vector &result );

// Store the min or max of each of x, y, and z into the result.
void XM_CALLCONV VectorMin( const Vector &a, const Vector &b, Vector &result );
void XM_CALLCONV VectorMax( const Vector &a, const Vector &b, Vector &result );

// Linearly interpolate between two vectors
void XM_CALLCONV VectorLerp(const Vector& src1, const Vector& src2, vec_t t, Vector& dest );
Vector XM_CALLCONV VectorLerp(const Vector& src1, const Vector& src2, vec_t t );

FORCEINLINE Vector XM_CALLCONV ReplicateToVector( float x )
{
	return Vector( x, x, x );
}

// check if a point is in the field of a view of an object. supports up to 180 degree fov.
FORCEINLINE bool XM_CALLCONV PointWithinViewAngle( Vector const &vecSrcPosition, 
												   Vector const &vecTargetPosition, 
												   Vector const &vecLookDirection, float flCosHalfFOV )
{
	Vector vecDelta = vecTargetPosition - vecSrcPosition;
	float cosDiff = DotProduct( vecLookDirection, vecDelta );

	if ( cosDiff < 0 ) 
		return false;

	float flLen2 = vecDelta.LengthSqr();

	// a/sqrt(b) > c  == a^2 > b * c ^2
	return ( cosDiff * cosDiff > flLen2 * flCosHalfFOV * flCosHalfFOV );
}


#ifndef VECTOR_NO_SLOW_OPERATIONS

// Cross product
Vector XM_CALLCONV CrossProduct( const Vector& a, const Vector& b );

// Random vector creation
Vector XM_CALLCONV RandomVector( vec_t minVal, vec_t maxVal );

#endif

float XM_CALLCONV RandomVectorInUnitSphere( Vector *pVector );
float XM_CALLCONV RandomVectorInUnitCircle( Vector2D *pVector );


//-----------------------------------------------------------------------------
//
// Inlined Vector methods
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// constructors
//-----------------------------------------------------------------------------
inline Vector::Vector(vec_t X, vec_t Y, vec_t Z)
	: x{X}, y{Y}, z{Z}
{ 
	CHECK_VALID(*this);
}

inline Vector::Vector(vec_t XYZ)
	: x{XYZ}, y{XYZ}, z{XYZ}
{
	CHECK_VALID(*this);
}

//inline Vector::Vector(const float *pFloat)
//{
//	Assert( pFloat );
//	x = pFloat[0]; y = pFloat[1]; z = pFloat[2];
//	CHECK_VALID(*this);
//} 

//-----------------------------------------------------------------------------
// initialization
//-----------------------------------------------------------------------------

inline void Vector::Init( vec_t ix, vec_t iy, vec_t iz )
{ 
	x = ix; y = iy; z = iz;
	CHECK_VALID(*this);
}

inline void Vector::Random( vec_t minVal, vec_t maxVal )
{
	DirectX::XMVECTOR rn = DirectX::XMVectorSet
	(
		(static_cast<float>(rand()) / VALVE_RAND_MAX),
		(static_cast<float>(rand()) / VALVE_RAND_MAX),
		(static_cast<float>(rand()) / VALVE_RAND_MAX),
		0.0f
	);
	DirectX::XMVECTOR mn = DirectX::XMVectorReplicate( minVal );

	DirectX::XMStoreFloat3
	(
		XmBase(),
		DirectX::XMVectorMultiplyAdd
		(
			rn,
			DirectX::XMVectorSubtract( DirectX::XMVectorReplicate( maxVal ), mn ),
			mn
		)
	);

	CHECK_VALID(*this);
}

// This should really be a single opcode on the PowerPC (move r0 onto the vec reg)
inline void Vector::Zero()
{
	x = y = z = 0.0f;
}

inline void XM_CALLCONV VectorClear( Vector& a )
{
	a.x = a.y = a.z = 0.0f;
}

//-----------------------------------------------------------------------------
// Array access
//-----------------------------------------------------------------------------
inline vec_t& Vector::operator[](int i)
{
	Assert( (i >= 0) && (i < 3) );
	static_assert(alignof(Vector) == alignof(vec_t));
	return reinterpret_cast<vec_t*>(this)[i];
}

inline vec_t Vector::operator[](int i) const
{
	Assert( (i >= 0) && (i < 3) );
	static_assert(alignof(Vector) == alignof(vec_t));
	return reinterpret_cast<const vec_t*>(this)[i];
}


//-----------------------------------------------------------------------------
// Base address...
//-----------------------------------------------------------------------------
inline vec_t* Vector::Base()
{
	static_assert(alignof(Vector) == alignof(vec_t));
	return reinterpret_cast<vec_t*>(this);
}

inline vec_t const* Vector::Base() const
{
	static_assert(alignof(Vector) == alignof(vec_t));
	return reinterpret_cast<const vec_t*>(this);
}

//-----------------------------------------------------------------------------
// Cast to Vector2D...
//-----------------------------------------------------------------------------

inline Vector2D& Vector::AsVector2D()
{
	static_assert(alignof(Vector) == alignof(Vector2D));
	return *reinterpret_cast<Vector2D*>(this);
}

inline const Vector2D& Vector::AsVector2D() const
{
	static_assert(alignof(Vector) == alignof(Vector2D));
	return *reinterpret_cast<const Vector2D*>(this);
}

//-----------------------------------------------------------------------------
// IsValid?
//-----------------------------------------------------------------------------

inline bool Vector::IsValid() const
{
	return !DirectX::XMVector3IsNaN( DirectX::XMLoadFloat3( XmBase() ) );
}

//-----------------------------------------------------------------------------
// Invalidate
//-----------------------------------------------------------------------------

inline void Vector::Invalidate()
{
//#ifdef _DEBUG
//#ifdef VECTOR_PARANOIA
	DirectX::XMStoreFloat3( XmBase(), DirectX::XMVectorSplatQNaN() );
//#endif
//#endif
}

//-----------------------------------------------------------------------------
// comparison
//-----------------------------------------------------------------------------

inline bool Vector::operator==( const Vector& src ) const
{
	CHECK_VALID(src);
	CHECK_VALID(*this);

	return DirectX::XMVector3Equal
	(
		DirectX::XMLoadFloat3( XmBase() ),
		DirectX::XMLoadFloat3( src.XmBase() )
	);
}

inline bool Vector::operator!=( const Vector& src ) const
{
	return !(*this == src);
}


//-----------------------------------------------------------------------------
// Copy
//-----------------------------------------------------------------------------

FORCEINLINE void XM_CALLCONV VectorCopy( const Vector& src, Vector& dst )
{
	CHECK_VALID(src);
	dst.x = src.x;
	dst.y = src.y;
	dst.z = src.z;
}

inline void	Vector::CopyToArray(float* rgfl) const
{ 
	Assert( rgfl );
	CHECK_VALID(*this);
	rgfl[0] = x; rgfl[1] = y; rgfl[2] = z; 
}

//-----------------------------------------------------------------------------
// standard math operations
//-----------------------------------------------------------------------------
// #pragma message("TODO: these should be SSE")

inline void Vector::Negate()
{ 
	CHECK_VALID(*this);
	x = -x; y = -y; z = -z;
} 

FORCEINLINE  Vector& Vector::operator+=(const Vector& v)
{ 
	CHECK_VALID(*this);
	CHECK_VALID(v);
	x += v.x; y += v.y; z += v.z;
	return *this;
}

FORCEINLINE  Vector& Vector::operator-=(const Vector& v)
{ 
	CHECK_VALID(*this);
	CHECK_VALID(v);
	x -= v.x; y -= v.y; z -= v.z;
	return *this;
}

FORCEINLINE  Vector& Vector::operator*=(float fl)
{
	x *= fl;
	y *= fl;
	z *= fl;
	CHECK_VALID(*this);
	return *this;
}

FORCEINLINE  Vector& Vector::operator*=(const Vector& v)
{ 
	CHECK_VALID(v);
	x *= v.x;
	y *= v.y;
	z *= v.z;
	CHECK_VALID(*this);
	return *this;
}

// this ought to be an opcode.
FORCEINLINE Vector&	Vector::operator+=(float fl)
{
	x += fl;
	y += fl;
	z += fl;
	CHECK_VALID(*this);
	return *this;
}

FORCEINLINE Vector&	Vector::operator-=(float fl)
{
	x -= fl;
	y -= fl;
	z -= fl;
	CHECK_VALID(*this);
	return *this;
}



FORCEINLINE  Vector& Vector::operator/=(float fl)
{
	Assert( fl != 0.0f );
	float oofl = 1.0f / fl;
	x *= oofl;
	y *= oofl;
	z *= oofl;
	CHECK_VALID(*this);
	return *this;
}

FORCEINLINE  Vector& Vector::operator/=(const Vector& v)
{ 
	CHECK_VALID(v);
	Assert( v.x != 0.0f && v.y != 0.0f && v.z != 0.0f );
	x /= v.x;
	y /= v.y;
	z /= v.z;
	CHECK_VALID(*this);
	return *this;
}



//-----------------------------------------------------------------------------
//
// Inlined Short Vector methods
//
//-----------------------------------------------------------------------------


inline void ShortVector::Init( short ix, short iy, short iz, short iw )
{ 
	x = ix; y = iy; z = iz; w = iw;
}

FORCEINLINE void ShortVector::Set( const ShortVector& vOther )
{
   x = vOther.x;
   y = vOther.y;
   z = vOther.z;
   w = vOther.w;
}

FORCEINLINE void ShortVector::Set( const short ix, const short iy, const short iz, const short iw )
{
   x = ix;
   y = iy;
   z = iz;
   w = iw;
}


//-----------------------------------------------------------------------------
// Array access
//-----------------------------------------------------------------------------
inline short ShortVector::operator[](int i) const
{
	Assert( (i >= 0) && (i < 4) );
	static_assert(alignof(ShortVector) >= alignof(short) &&
		alignof(ShortVector) % alignof(short) >= 0);
	return reinterpret_cast<const short*>(this)[i];
}

inline short& ShortVector::operator[](int i)
{
	Assert( (i >= 0) && (i < 4) );
	static_assert(alignof(ShortVector) >= alignof(short) &&
		alignof(ShortVector) % alignof(short) >= 0);
	return reinterpret_cast<short*>(this)[i];
}

//-----------------------------------------------------------------------------
// Base address...
//-----------------------------------------------------------------------------
inline short* ShortVector::Base()
{
	static_assert(alignof(ShortVector) >= alignof(short) &&
		alignof(ShortVector) % alignof(short) >= 0);
	return reinterpret_cast<short*>(this);
}

inline short const* ShortVector::Base() const
{
	static_assert(alignof(ShortVector) >= alignof(short) &&
		alignof(ShortVector) % alignof(short) >= 0);
	return reinterpret_cast<const short*>(this);
}


//-----------------------------------------------------------------------------
// comparison
//-----------------------------------------------------------------------------

inline bool ShortVector::operator==( const ShortVector& src ) const
{
	return (src.x == x) && (src.y == y) && (src.z == z) && (src.w == w);
}

inline bool ShortVector::operator!=( const ShortVector& src ) const
{
	return (src.x != x) || (src.y != y) || (src.z != z) || (src.w != w);
}



//-----------------------------------------------------------------------------
// standard math operations
//-----------------------------------------------------------------------------

FORCEINLINE  ShortVector& ShortVector::operator+=(const ShortVector& v)
{ 
	x+=v.x; y+=v.y; z += v.z; w += v.w;
	return *this;
}

FORCEINLINE  ShortVector& ShortVector::operator-=(const ShortVector& v)
{ 
	x-=v.x; y-=v.y; z -= v.z; w -= v.w;
	return *this;
}

FORCEINLINE  ShortVector& ShortVector::operator*=(float fl)
{
	x = static_cast<short>(x * fl);
	y = static_cast<short>(y * fl);
	z = static_cast<short>(z * fl);
	w = static_cast<short>(w * fl);
	return *this;
}

FORCEINLINE  ShortVector& ShortVector::operator*=(const ShortVector& v)
{ 
	x *= v.x;
	y *= v.y;
	z *= v.z;
	w *= v.w;
	return *this;
}

FORCEINLINE  ShortVector& ShortVector::operator/=(float fl)
{
	Assert( fl != 0.0f );
	const float oofl = 1.0f / fl;
	x = static_cast<short>(x * oofl);
	y = static_cast<short>(y * oofl);
	z = static_cast<short>(z * oofl);
	w = static_cast<short>(w * oofl);
	return *this;
}

FORCEINLINE  ShortVector& ShortVector::operator/=(const ShortVector& v)
{ 
	Assert( v.x != 0 && v.y != 0 && v.z != 0 && v.w != 0 );
	x /= v.x;
	y /= v.y;
	z /= v.z;
	w /= v.w;
	return *this;
}

FORCEINLINE void ShortVectorMultiply( const ShortVector& src, float fl, ShortVector& res )
{
	Assert( IsFinite(fl) );
	res.x = static_cast<short>(src.x * fl);
	res.y = static_cast<short>(src.y * fl);
	res.z = static_cast<short>(src.z * fl);
	res.w = static_cast<short>(src.w * fl);
}

FORCEINLINE ShortVector ShortVector::operator*(float fl) const
{ 
	ShortVector res;
	ShortVectorMultiply( *this, fl, res );
	return res;	
}






//-----------------------------------------------------------------------------
//
// Inlined Integer Vector methods
//
//-----------------------------------------------------------------------------


inline void IntVector4D::Init( int ix, int iy, int iz, int iw )
{ 
	x = ix; y = iy; z = iz; w = iw;
}

FORCEINLINE void IntVector4D::Set( const IntVector4D& vOther )
{
	x = vOther.x;
	y = vOther.y;
	z = vOther.z;
	w = vOther.w;
}

FORCEINLINE void IntVector4D::Set( const int ix, const int iy, const int iz, const int iw )
{
	x = ix;
	y = iy;
	z = iz;
	w = iw;
}


//-----------------------------------------------------------------------------
// Array access
//-----------------------------------------------------------------------------
inline int IntVector4D::operator[](int i) const
{
	Assert( (i >= 0) && (i < 4) );
	static_assert(alignof(IntVector4D) == alignof(int));
	return reinterpret_cast<const int *>(this)[i];
}

inline int& IntVector4D::operator[](int i)
{
	Assert( (i >= 0) && (i < 4) );
	static_assert(alignof(IntVector4D) == alignof(int));
	return reinterpret_cast<int*>(this)[i];
}

//-----------------------------------------------------------------------------
// Base address...
//-----------------------------------------------------------------------------
inline int* IntVector4D::Base()
{
	static_assert(alignof(IntVector4D) == alignof(int));
	return reinterpret_cast<int*>(this);
}

inline int const* IntVector4D::Base() const
{
	static_assert(alignof(IntVector4D) == alignof(int));
	return reinterpret_cast<const int*>(this);
}


//-----------------------------------------------------------------------------
// comparison
//-----------------------------------------------------------------------------

inline bool IntVector4D::operator==( const IntVector4D& src ) const
{
	return (src.x == x) && (src.y == y) && (src.z == z) && (src.w == w);
}

inline bool IntVector4D::operator!=( const IntVector4D& src ) const
{
	return (src.x != x) || (src.y != y) || (src.z != z) || (src.w != w);
}



//-----------------------------------------------------------------------------
// standard math operations
//-----------------------------------------------------------------------------

FORCEINLINE  IntVector4D& IntVector4D::operator+=(const IntVector4D& v)
{ 
	x+=v.x; y+=v.y; z += v.z; w += v.w;
	return *this;
}

FORCEINLINE  IntVector4D& IntVector4D::operator-=(const IntVector4D& v)
{ 
	x-=v.x; y-=v.y; z -= v.z; w -= v.w;
	return *this;
}

FORCEINLINE  IntVector4D& IntVector4D::operator*=(float fl)
{
  x = static_cast<int>(x * fl);
  y = static_cast<int>(y * fl);
  z = static_cast<int>(z * fl);
  w = static_cast<int>(w * fl);
	return *this;
}

FORCEINLINE  IntVector4D& IntVector4D::operator*=(const IntVector4D& v)	
{ 
	x *= v.x;
	y *= v.y;
	z *= v.z;
	w *= v.w;
	return *this;
}

FORCEINLINE  IntVector4D& IntVector4D::operator/=(float fl)
{
	Assert( fl != 0.0f );
	const float oofl = 1.0f / fl;
	x = static_cast<int>(x * oofl);
	y = static_cast<int>(y * oofl);
	z = static_cast<int>(z * oofl);
	w = static_cast<int>(w * oofl);
	return *this;
}

FORCEINLINE  IntVector4D& IntVector4D::operator/=(const IntVector4D& v)
{ 
	Assert( v.x != 0 && v.y != 0 && v.z != 0 && v.w != 0 );
	x /= v.x;
	y /= v.y;
	z /= v.z;
	w /= v.w;
	return *this;
}

FORCEINLINE void IntVector4DMultiply( const IntVector4D& src, float fl, IntVector4D& res )
{
	Assert( IsFinite(fl) );
	res.x = static_cast<const int>(src.x * fl);
	res.y = static_cast<const int>(src.y * fl);
	res.z = static_cast<const int>(src.z * fl);
	res.w = static_cast<const int>(src.w * fl);
}

FORCEINLINE IntVector4D IntVector4D::operator*(float fl) const
{ 
	IntVector4D res;
	IntVector4DMultiply( *this, fl, res );
	return res;	
}



// =======================


FORCEINLINE void XM_CALLCONV VectorAdd( const Vector& a, const Vector& b, Vector& c )
{
	CHECK_VALID(a);
	CHECK_VALID(b);

	DirectX::XMStoreFloat3
	(
		c.XmBase(),
		DirectX::XMVectorAdd( DirectX::XMLoadFloat3( a.XmBase() ), DirectX::XMLoadFloat3( b.XmBase() ) )
	);
}

FORCEINLINE void XM_CALLCONV VectorSubtract( const Vector& a, const Vector& b, Vector& c )
{
	CHECK_VALID(a);
	CHECK_VALID(b);

	DirectX::XMStoreFloat3
	(
		c.XmBase(),
		DirectX::XMVectorSubtract( DirectX::XMLoadFloat3( a.XmBase() ), DirectX::XMLoadFloat3( b.XmBase() ) )
	);
}

FORCEINLINE void XM_CALLCONV VectorMultiply( const Vector& a, vec_t b, Vector& c )
{
	CHECK_VALID(a);
	Assert( IsFinite(b) );

	DirectX::XMStoreFloat3
	(
		c.XmBase(),
		DirectX::XMVectorScale( DirectX::XMLoadFloat3( a.XmBase() ), b )
	);
}

FORCEINLINE void XM_CALLCONV VectorMultiply( const Vector& a, const Vector& b, Vector& c )
{
	CHECK_VALID(a);
	CHECK_VALID(b);

	DirectX::XMStoreFloat3
	(
		c.XmBase(),
		DirectX::XMVectorMultiply
		(
			DirectX::XMLoadFloat3( a.XmBase() ),
			DirectX::XMLoadFloat3( b.XmBase() )
		)
	);
}

// for backwards compatability
inline void XM_CALLCONV VectorScale ( const Vector& in, vec_t scale, Vector& result )
{
	VectorMultiply( in, scale, result );
}


FORCEINLINE void XM_CALLCONV VectorDivide( const Vector& a, vec_t b, Vector& c )
{
	CHECK_VALID(a);
	Assert( b != 0.0f );

	VectorMultiply( a, 1.0f / b, c );
}

FORCEINLINE void XM_CALLCONV VectorDivide( const Vector& a, const Vector& b, Vector& c )
{
	CHECK_VALID(a);
	CHECK_VALID(b);
	Assert( (b.x != 0.0f) && (b.y != 0.0f) && (b.z != 0.0f) );

	DirectX::XMStoreFloat3
	(
		c.XmBase(),
		DirectX::XMVectorDivide
		(
			DirectX::XMLoadFloat3( a.XmBase() ),
			// Ensure no zero division on W.
			DirectX::XMVectorSetW( DirectX::XMLoadFloat3( b.XmBase() ), 1.0f )
		)
	);
}

// FIXME: Remove
// For backwards compatability
inline void	Vector::MulAdd(const Vector& a, const Vector& b, float scalar)
{
	CHECK_VALID(a);
	CHECK_VALID(b);

	DirectX::XMStoreFloat3
	(
		XmBase(),
		DirectX::XMVectorMultiplyAdd
		(
			DirectX::XMLoadFloat3( b.XmBase() ),
			DirectX::XMVectorReplicate( scalar ),
			DirectX::XMLoadFloat3( a.XmBase() )
		)
	);
}

inline void XM_CALLCONV VectorLerp(const Vector& src1, const Vector& src2, vec_t t, Vector& dest )
{
	CHECK_VALID(src1);
	CHECK_VALID(src2);

	DirectX::XMStoreFloat3
	(
		dest.XmBase(),
		DirectX::XMVectorLerp
		(
			DirectX::XMLoadFloat3( src1.XmBase() ),
			DirectX::XMLoadFloat3( src2.XmBase() ),
			t
		)
	);
}

inline Vector XM_CALLCONV VectorLerp(const Vector& src1, const Vector& src2, vec_t t )
{
	Vector result;
	VectorLerp( src1, src2, t, result );
	return result;
}

//-----------------------------------------------------------------------------
// Temporary storage for vector results so const Vector& results can be returned
//-----------------------------------------------------------------------------
inline Vector& XM_CALLCONV AllocTempVector()
{
	static Vector s_vecTemp[128];
	static CInterlockedInt s_nIndex;

	int nIndex;
	for (;;)
	{
		int nOldIndex = s_nIndex;
		nIndex = ( (nOldIndex + 0x10001) & 0x7F );

		if ( s_nIndex.AssignIf( nOldIndex, nIndex ) )
		{
			break;
		}
		ThreadPause();
	} 
	return s_vecTemp[nIndex];
}



//-----------------------------------------------------------------------------
// dot, cross
//-----------------------------------------------------------------------------
FORCEINLINE vec_t XM_CALLCONV DotProduct(const Vector& a, const Vector& b) 
{ 
	CHECK_VALID(a);
	CHECK_VALID(b);

	return DirectX::XMVectorGetX
	(
		DirectX::XMVector3Dot
		(
			DirectX::XMLoadFloat3( a.XmBase() ),
			DirectX::XMLoadFloat3( b.XmBase() )
		)
	);
}

// for backwards compatability
inline vec_t Vector::Dot( const Vector& vOther ) const
{
	CHECK_VALID(vOther);
	return DotProduct( *this, vOther );
}

inline void XM_CALLCONV CrossProduct( const Vector& a, const Vector& b, Vector& result )
{
	CHECK_VALID(a);
	CHECK_VALID(b);
	Assert( &a != &result );
	Assert( &b != &result );

	DirectX::XMStoreFloat3
	(
		result.XmBase(),
		DirectX::XMVector3Cross
		(
			DirectX::XMLoadFloat3( a.XmBase() ),
			DirectX::XMLoadFloat3( b.XmBase() )
		)
	);
}

inline void XM_CALLCONV CrossProduct( const Vector& a, const Vector& b, DirectX::XMFLOAT4 *result )
{
	CHECK_VALID(a);
	CHECK_VALID(b);
	Assert(result);

	DirectX::XMStoreFloat4
	(
		result,
		DirectX::XMVector3Cross
		(
			DirectX::XMLoadFloat3( a.XmBase() ),
			DirectX::XMLoadFloat3( b.XmBase() )
		)
	);
}

inline void XM_CALLCONV CrossProduct(const DirectX::XMFLOAT4 *a, const Vector& b, DirectX::XMFLOAT4 *result )
{
	Assert(a);
	CHECK_VALID(b);
	Assert(result);

	DirectX::XMStoreFloat4
	(
		result,
		DirectX::XMVector3Cross
		(
			DirectX::XMLoadFloat4( a ),
			DirectX::XMLoadFloat3( b.XmBase() )
		)
	);
}

inline void XM_CALLCONV CrossProduct(const DirectX::XMFLOAT4 *a, const Vector& b, Vector &result )
{
	Assert(a);
	CHECK_VALID(b);

	DirectX::XMStoreFloat3
	(
		result.XmBase(),
		DirectX::XMVector3Cross
		(
			DirectX::XMLoadFloat4( a ),
			DirectX::XMLoadFloat3( b.XmBase() )
		)
	);
}

inline vec_t XM_CALLCONV DotProductAbs( const Vector &v0, const DirectX::XMFLOAT4 *v1 )
{
	CHECK_VALID(v0);
	Assert(v1);

	return DirectX::XMVectorGetX
	(
		DirectX::XMVectorSum
		(
			DirectX::XMVectorAbs
			(
				DirectX::XMVectorMultiply
				(
					DirectX::XMLoadFloat3( v0.XmBase() ),
					DirectX::XMLoadFloat4( v1 )
				)
			)
		)
	);
}

inline vec_t XM_CALLCONV DotProductAbs( const Vector &v0, const Vector &v1 )
{
	CHECK_VALID(v0);
	CHECK_VALID(v1);

	return DirectX::XMVectorGetX
	(
		DirectX::XMVectorSum
		(
			DirectX::XMVectorAbs
			(
				DirectX::XMVectorMultiply
				(
					DirectX::XMLoadFloat3( v0.XmBase() ),
					DirectX::XMLoadFloat3( v1.XmBase() )
				)
			)
		)
	);
}

// dimhotepus: Too error prone. Use safer XMFLOAT3 variant above.
inline vec_t DotProductAbs( const Vector &v0, const float *v1 ) = delete;

//-----------------------------------------------------------------------------
// length
//-----------------------------------------------------------------------------

inline vec_t XM_CALLCONV VectorLength( const Vector& v )
{
	CHECK_VALID(v);

	return DirectX::XMVectorGetX
	(
		DirectX::XMVector3Length
		(
			DirectX::XMLoadFloat3( v.XmBase() )
		)
	);
}


inline vec_t Vector::Length(void) const
{
	CHECK_VALID(*this);
	return VectorLength( *this );
}


//-----------------------------------------------------------------------------
// Normalization
//-----------------------------------------------------------------------------

// check a point against a box
bool Vector::WithinAABox( Vector const &boxmin, Vector const &boxmax) const
{
	DirectX::XMVECTOR bmin = DirectX::XMLoadFloat3( boxmin.XmBase() );
	DirectX::XMVECTOR bmax = DirectX::XMLoadFloat3( boxmax.XmBase() );
	DirectX::XMVECTOR self = DirectX::XMLoadFloat3( XmBase() );

	return DirectX::XMVector3GreaterOrEqual( self, bmin ) &&
		DirectX::XMVector3LessOrEqual( self, bmax );
}

//-----------------------------------------------------------------------------
// Get the distance from this vector to the other one 
//-----------------------------------------------------------------------------
inline vec_t Vector::DistTo(const Vector &vOther) const
{
	Vector delta;
	VectorSubtract( *this, vOther, delta );
	return delta.Length();
}


//-----------------------------------------------------------------------------
// Vector equality with tolerance
//-----------------------------------------------------------------------------
inline bool XM_CALLCONV VectorsAreEqual( const Vector& src1, const Vector& src2, float tolerance )
{
	return DirectX::XMVector3NearEqual
	(
		DirectX::XMLoadFloat3( src1.XmBase() ),
		DirectX::XMLoadFloat3( src2.XmBase() ),
		DirectX::XMVectorReplicate( tolerance )
	);
}


//-----------------------------------------------------------------------------
// Computes the closest point to vecTarget no farther than flMaxDist from vecStart
//-----------------------------------------------------------------------------
inline void XM_CALLCONV ComputeClosestPoint( const Vector& vecStart, float flMaxDist, const Vector& vecTarget, Vector *pResult )
{
	Vector vecDelta;
	VectorSubtract( vecTarget, vecStart, vecDelta );
	float flDistSqr = vecDelta.LengthSqr();
	if ( flDistSqr <= flMaxDist * flMaxDist )
	{
		*pResult = vecTarget;
	}
	else
	{
		vecDelta /= FastSqrt( flDistSqr );
		VectorMA( vecStart, flMaxDist, vecDelta, *pResult );
	}
}


//-----------------------------------------------------------------------------
// Takes the absolute value of a vector
//-----------------------------------------------------------------------------
inline void XM_CALLCONV VectorAbs( const Vector& src, Vector& dst )
{
	DirectX::XMStoreFloat3
	(
		dst.XmBase(),
		DirectX::XMVectorAbs( DirectX::XMLoadFloat3( src.XmBase() ) )
	);
}


//-----------------------------------------------------------------------------
//
// Slow methods
//
//-----------------------------------------------------------------------------

#ifndef VECTOR_NO_SLOW_OPERATIONS

//-----------------------------------------------------------------------------
// Returns a vector with the min or max in X, Y, and Z.
//-----------------------------------------------------------------------------
inline Vector Vector::Min(const Vector &vOther) const
{
	DirectX::XMVECTOR result = DirectX::XMVectorMin
	(
		DirectX::XMLoadFloat3( XmBase() ),
		DirectX::XMLoadFloat3( vOther.XmBase() )
	);

	Vector r;
	DirectX::XMStoreFloat3( r.XmBase(), result );
	return r;
}

inline Vector Vector::Max(const Vector &vOther) const
{
	DirectX::XMVECTOR result = DirectX::XMVectorMax
	(
		DirectX::XMLoadFloat3( XmBase() ),
		DirectX::XMLoadFloat3( vOther.XmBase() )
	);

	Vector r;
	DirectX::XMStoreFloat3( r.XmBase(), result );
	return r;
}


//-----------------------------------------------------------------------------
// arithmetic operations
//-----------------------------------------------------------------------------

inline Vector Vector::operator-(void) const
{ 
	DirectX::XMVECTOR result = DirectX::XMVectorNegate
	(
		DirectX::XMLoadFloat3( XmBase() )
	);
	
	Vector r;
	DirectX::XMStoreFloat3( r.XmBase(), result );
	return r;
}

inline Vector Vector::operator+(const Vector& v) const	
{ 
	Vector res;
	VectorAdd( *this, v, res );
	return res;	
}

inline Vector Vector::operator-(const Vector& v) const	
{ 
	Vector res;
	VectorSubtract( *this, v, res );
	return res;	
}

inline Vector Vector::operator*(float fl) const	
{ 
	Vector res;
	VectorMultiply( *this, fl, res );
	return res;	
}

inline Vector Vector::operator*(const Vector& v) const	
{ 
	Vector res;
	VectorMultiply( *this, v, res );
	return res;	
}

inline Vector Vector::operator/(float fl) const	
{ 
	Vector res;
	VectorDivide( *this, fl, res );
	return res;	
}

inline Vector Vector::operator/(const Vector& v) const	
{ 
	Vector res;
	VectorDivide( *this, v, res );
	return res;	
}

inline Vector operator*(float fl, const Vector& v)	
{ 
	return v * fl; 
}

//-----------------------------------------------------------------------------
// cross product
//-----------------------------------------------------------------------------

inline Vector Vector::Cross(const Vector& vOther) const
{ 
	Vector res;
	CrossProduct( *this, vOther, res );
	return res;
}

//-----------------------------------------------------------------------------
// 2D
//-----------------------------------------------------------------------------

inline vec_t Vector::Length2D(void) const
{ 
	return FastSqrt(x*x + y*y); 
}

inline vec_t Vector::Length2DSqr(void) const
{ 
	return (x*x + y*y); 
}

inline Vector XM_CALLCONV CrossProduct(const Vector& a, const Vector& b) 
{
	DirectX::XMVECTOR result = DirectX::XMVector3Cross
	(
		DirectX::XMLoadFloat3( a.XmBase() ),
		DirectX::XMLoadFloat3( b.XmBase() )
	);

	Vector r;
	DirectX::XMStoreFloat3( r.XmBase(), result );
	return r;
}

inline void XM_CALLCONV VectorMin( const Vector &a, const Vector &b, Vector &result )
{
	DirectX::XMVECTOR r = DirectX::XMVectorMin
	(
		DirectX::XMLoadFloat3( a.XmBase() ),
		DirectX::XMLoadFloat3( b.XmBase() )
	);

	DirectX::XMStoreFloat3( result.XmBase(), r );
}

inline void XM_CALLCONV VectorMax( const Vector &a, const Vector &b, Vector &result )
{
	DirectX::XMVECTOR r = DirectX::XMVectorMax
	(
		DirectX::XMLoadFloat3( a.XmBase() ),
		DirectX::XMLoadFloat3( b.XmBase() )
	);

	DirectX::XMStoreFloat3( result.XmBase(), r );
}

inline float XM_CALLCONV ComputeVolume( const Vector &vecMins, const Vector &vecMaxs )
{
	Vector vecDelta;
	VectorSubtract( vecMaxs, vecMins, vecDelta );
	return DotProduct( vecDelta, vecDelta );
}

// Get a random vector.
inline Vector XM_CALLCONV RandomVector( float minVal, float maxVal )
{
	Vector vRandom;
	vRandom.Random( minVal, maxVal );
	return vRandom;
}

#endif //slow

//-----------------------------------------------------------------------------
// Helper debugging stuff....
//-----------------------------------------------------------------------------

inline bool operator==( float const*, const Vector& ) = delete;
inline bool operator==( const Vector&, float const* ) = delete;
inline bool operator!=( float const*, const Vector& ) = delete;
inline bool operator!=( const Vector&, float const* ) = delete;

//-----------------------------------------------------------------------------
// AngularImpulse
//-----------------------------------------------------------------------------
// AngularImpulse are exponetial maps (an axis scaled by a "twist" angle in degrees)
typedef Vector AngularImpulse;

#ifndef VECTOR_NO_SLOW_OPERATIONS

inline AngularImpulse XM_CALLCONV RandomAngularImpulse( float minVal, float maxVal )
{
	AngularImpulse	angImp;
	angImp.Random( minVal, maxVal );
	return angImp;
}

#endif


//-----------------------------------------------------------------------------
// Quaternion
//-----------------------------------------------------------------------------

class RadianEuler;

// same data-layout as engine's vec4_t,	which is a vec_t[4]

// dimhotepus: alignas to use vec_t operator[] safely.
// dimhotepus: XM_CALLCONV to speedup calls.
class alignas(vec_t) Quaternion
{
public:
#if defined(_DEBUG) && defined(VECTOR_PARANOIA)
	// Initialize to NAN to catch errors
	Quaternion() : x(VEC_T_NAN), y(VEC_T_NAN), z(VEC_T_NAN), w(VEC_T_NAN)
	{
	}
#else
	Quaternion() = default;
#endif
	inline Quaternion(vec_t ix, vec_t iy, vec_t iz, vec_t iw) : x(ix), y(iy), z(iz), w(iw) { }
	inline Quaternion(RadianEuler const &angle);	// evil auto type promotion!!!

	inline void XM_CALLCONV Init(vec_t ix=0.0f, vec_t iy=0.0f, vec_t iz=0.0f, vec_t iw=0.0f)	{ x = ix; y = iy; z = iz; w = iw; }

	bool XM_CALLCONV  IsValid() const;
	void Invalidate();

	bool XM_CALLCONV operator==( const Quaternion &src ) const;
	bool XM_CALLCONV operator!=( const Quaternion &src ) const;

	vec_t* XM_CALLCONV Base()
	{
		static_assert(alignof(Quaternion) == alignof(vec_t));
		return reinterpret_cast<vec_t*>(this);
	}
	const vec_t* XM_CALLCONV Base() const
	{
		static_assert(alignof(Quaternion) == alignof(vec_t));
		return reinterpret_cast<const vec_t*>(this);
	}

	// dimhotepus: Better DirectX math integration.
	DirectX::XMFLOAT4* XM_CALLCONV XmBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT4) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT4) == alignof(Quaternion));
		return reinterpret_cast<DirectX::XMFLOAT4*>(this);
	}
	DirectX::XMFLOAT4 const* XM_CALLCONV XmBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT4) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT4) == alignof(Quaternion));
		return reinterpret_cast<DirectX::XMFLOAT4 const*>(this);
	}

	// array access...
	vec_t XM_CALLCONV operator[](int i) const;
	vec_t& XM_CALLCONV operator[](int i);

	vec_t x, y, z, w;
};


//-----------------------------------------------------------------------------
// Array access
//-----------------------------------------------------------------------------
inline vec_t& Quaternion::operator[](int i)
{
	Assert( (i >= 0) && (i < 4) );
	static_assert(alignof(Quaternion) == alignof(vec_t));
	return reinterpret_cast<vec_t*>(this)[i];
}

inline vec_t Quaternion::operator[](int i) const
{
	Assert( (i >= 0) && (i < 4) );
	static_assert(alignof(Quaternion) == alignof(vec_t));
	return reinterpret_cast<const vec_t*>(this)[i];
}


//-----------------------------------------------------------------------------
// Equality test
//-----------------------------------------------------------------------------
inline bool Quaternion::operator==( const Quaternion &src ) const
{
	return DirectX::XMVector4Equal
	(
		DirectX::XMLoadFloat4( XmBase() ),
		DirectX::XMLoadFloat4( src.XmBase() )
	);
}

inline bool Quaternion::operator!=( const Quaternion &src ) const
{
	return !(*this == src);
}


//-----------------------------------------------------------------------------
// Quaternion equality with tolerance
//-----------------------------------------------------------------------------
inline bool XM_CALLCONV QuaternionsAreEqual( const Quaternion& src1, const Quaternion& src2, float tolerance )
{
	return DirectX::XMVector4NearEqual
	(
		DirectX::XMLoadFloat4( src1.XmBase() ),
		DirectX::XMLoadFloat4( src2.XmBase() ),
		DirectX::XMVectorReplicate( tolerance )
	);
}


//-----------------------------------------------------------------------------
// Here's where we add all those lovely SSE optimized routines
//-----------------------------------------------------------------------------
// dimhotepus: Fix aligned alloc.
class alignas(16) QuaternionAligned : public CAlignedNewDelete<16, Quaternion>
{
public:
	inline QuaternionAligned() = default;
	inline QuaternionAligned(vec_t X, vec_t Y, vec_t Z, vec_t W) 
	{
		Init(X,Y,Z,W);
	}

#ifdef VECTOR_NO_SLOW_OPERATIONS

private:
	// No copy constructors allowed if we're in optimal mode
	QuaternionAligned(const QuaternionAligned& vOther);
	QuaternionAligned(const Quaternion &vOther);

#else
public:
	explicit QuaternionAligned(const Quaternion &vOther) 
	{
		Init(vOther.x, vOther.y, vOther.z, vOther.w);
	}

	QuaternionAligned& XM_CALLCONV operator = (const Quaternion& vOther)
	{
		Init(vOther.x, vOther.y, vOther.z, vOther.w);
		return *this;
	}

	// dimhotepus: Better DirectX math integration.
	DirectX::XMFLOAT4A* XM_CALLCONV XmBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT4A) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT4A) == alignof(QuaternionAligned));
		return reinterpret_cast<DirectX::XMFLOAT4A*>(this);
	}
	DirectX::XMFLOAT4A const* XM_CALLCONV XmBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT4A) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT4A) == alignof(QuaternionAligned));
		return reinterpret_cast<DirectX::XMFLOAT4A const*>(this);
	}

#endif
};


//-----------------------------------------------------------------------------
// Radian Euler angle aligned to axis (NOT ROLL/PITCH/YAW)
//-----------------------------------------------------------------------------
class QAngle;

// dimhotepus: alignas to use vec_t operator[] safely.
// dimhotepus: XM_CALLCONV to speedup calls.
class alignas(vec_t) RadianEuler
{
public:
	inline RadianEuler() = default;
	inline RadianEuler(vec_t X, vec_t Y, vec_t Z)		{ x = X; y = Y; z = Z; }
	inline RadianEuler(Quaternion const &q);	// evil auto type promotion!!!
	inline RadianEuler(QAngle const &angles);	// evil auto type promotion!!!

	// Initialization
	inline void XM_CALLCONV Init(vec_t ix=0.0f, vec_t iy=0.0f, vec_t iz=0.0f)	{ x = ix; y = iy; z = iz; }

	// Conversion to qangle
	QAngle XM_CALLCONV ToQAngle() const;
	bool XM_CALLCONV IsValid() const;
	void Invalidate();

	// array access...
	vec_t XM_CALLCONV operator[](int i) const;
	vec_t& XM_CALLCONV operator[](int i);

	// dimhotepus: Better DirectX math integration.
	DirectX::XMFLOAT3* XM_CALLCONV XmBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT3) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT3) == alignof(RadianEuler));
		return reinterpret_cast<DirectX::XMFLOAT3*>(this);
	}
	DirectX::XMFLOAT3 const* XM_CALLCONV XmBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT3) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT3) == alignof(RadianEuler));
		return reinterpret_cast<DirectX::XMFLOAT3 const*>(this);
	}

	vec_t x, y, z;
};


extern void XM_CALLCONV AngleQuaternion( RadianEuler const &angles, Quaternion &qt );
extern void XM_CALLCONV QuaternionAngles( Quaternion const &q, RadianEuler &angles );

FORCEINLINE void XM_CALLCONV NetworkVarConstruct( Quaternion &q ) { q.x = q.y = q.z = q.w = 0.0f; }

inline Quaternion::Quaternion(RadianEuler const &angle) //-V730
{
	AngleQuaternion( angle, *this );
}

inline bool Quaternion::IsValid() const
{
	return !DirectX::XMVector4IsNaN( DirectX::XMLoadFloat4( XmBase() ) );
}

inline void Quaternion::Invalidate()
{
//#ifdef _DEBUG
//#ifdef VECTOR_PARANOIA
	DirectX::XMStoreFloat4( XmBase(), DirectX::XMVectorSplatQNaN() );
//#endif
//#endif
}

inline RadianEuler::RadianEuler(Quaternion const &q) //-V730
{
	QuaternionAngles( q, *this );
}

inline void XM_CALLCONV VectorCopy( RadianEuler const& src, RadianEuler &dst )
{
	CHECK_VALID(src);
	dst.x = src.x;
	dst.y = src.y;
	dst.z = src.z;
}

inline void XM_CALLCONV VectorScale( RadianEuler const& src, float b, RadianEuler &dst )
{
	CHECK_VALID(src);
	Assert( IsFinite(b) );

	DirectX::XMStoreFloat3
	(
		dst.XmBase(),
		DirectX::XMVectorScale( DirectX::XMLoadFloat3( src.XmBase() ), b )
	);
}

inline bool RadianEuler::IsValid() const
{
	return !DirectX::XMVector3IsNaN( DirectX::XMLoadFloat3( XmBase() ) );
}

inline void RadianEuler::Invalidate()
{
//#ifdef _DEBUG
//#ifdef VECTOR_PARANOIA
	DirectX::XMStoreFloat3( XmBase(), DirectX::XMVectorSplatQNaN() );
//#endif
//#endif
}


//-----------------------------------------------------------------------------
// Array access
//-----------------------------------------------------------------------------
inline vec_t& RadianEuler::operator[](int i)
{
	Assert( (i >= 0) && (i < 3) );
	static_assert(alignof(RadianEuler) == alignof(vec_t));
	return reinterpret_cast<vec_t*>(this)[i];
}

inline vec_t RadianEuler::operator[](int i) const
{
	Assert( (i >= 0) && (i < 3) );
	static_assert(alignof(RadianEuler) == alignof(vec_t));
	return reinterpret_cast<const vec_t*>(this)[i];
}


//-----------------------------------------------------------------------------
// Degree Euler QAngle pitch, yaw, roll
//-----------------------------------------------------------------------------
class QAngleByValue;

// dimhotepus: alignas to use vec_t operator[] safely.
// dimhotepus: XM_CALLCONV to speedup calls.
class alignas(vec_t) QAngle
{
public:
	// Members
	vec_t x, y, z;

	// Construction/destruction
#if defined(_DEBUG) && defined(VECTOR_PARANOIA)
    // Initialize to NAN to catch errors
	QAngle() : x(VEC_T_NAN), y(VEC_T_NAN), z(VEC_T_NAN)
	{
	}
#else
	QAngle() = default;
#endif
	QAngle(vec_t X, vec_t Y, vec_t Z);
//	QAngle(RadianEuler const &angles);	// evil auto type promotion!!!

	// Allow pass-by-value
	XM_CALLCONV operator QAngleByValue &()
	{
		//static_assert(alignof(QAngle) == alignof(QAngleByValue));
		return *reinterpret_cast<QAngleByValue *>(this);
	}
	XM_CALLCONV operator const QAngleByValue &() const
	{
		//static_assert(alignof(QAngle) == alignof(QAngleByValue));
		return *reinterpret_cast<const QAngleByValue *>(this);
	}

	// Initialization
	void XM_CALLCONV Init(vec_t ix=0.0f, vec_t iy=0.0f, vec_t iz=0.0f);
	void XM_CALLCONV Random( vec_t minVal, vec_t maxVal );

	// Got any nasty NAN's?
	bool XM_CALLCONV IsValid() const;
	void Invalidate();

	// array access...
	vec_t XM_CALLCONV operator[](int i) const;
	vec_t& XM_CALLCONV operator[](int i);

	// Base address...
	vec_t* XM_CALLCONV Base();
	vec_t const* XM_CALLCONV Base() const;

	// dimhotepus: Better DirectX math integration.
	DirectX::XMFLOAT3* XM_CALLCONV XmBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT3) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT3) == alignof(QAngle));
		return reinterpret_cast<DirectX::XMFLOAT3*>(this);
	}
	DirectX::XMFLOAT3 const* XM_CALLCONV XmBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT3) == sizeof(*this));
		static_assert(alignof(DirectX::XMFLOAT3) == alignof(QAngle));
		return reinterpret_cast<DirectX::XMFLOAT3 const*>(this);
	}
	
	// equality
	bool XM_CALLCONV operator==(const QAngle& v) const;
	bool XM_CALLCONV operator!=(const QAngle& v) const;	

	// arithmetic operations
	QAngle&	XM_CALLCONV operator+=(const QAngle &v);
	QAngle&	XM_CALLCONV operator-=(const QAngle &v);
	QAngle&	XM_CALLCONV operator*=(float s);
	QAngle&	XM_CALLCONV operator/=(float s);

	// Get the vector's magnitude.
	vec_t XM_CALLCONV Length() const;
	vec_t XM_CALLCONV LengthSqr() const;

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// copy constructors

	// arithmetic operations
	QAngle XM_CALLCONV operator-() const;
	
	QAngle XM_CALLCONV operator+(const QAngle& v) const;
	QAngle XM_CALLCONV operator-(const QAngle& v) const;
	QAngle XM_CALLCONV operator*(float fl) const;
	QAngle XM_CALLCONV operator/(float fl) const;
#else

private:
	// No copy constructors allowed if we're in optimal mode
	QAngle(const QAngle& vOther);

#endif
};

FORCEINLINE void XM_CALLCONV NetworkVarConstruct( QAngle &q ) { q.x = q.y = q.z = 0.0f; }

//-----------------------------------------------------------------------------
// Allows us to specifically pass the vector by value when we need to
//-----------------------------------------------------------------------------
class QAngleByValue : public QAngle
{
public:
	// Construction/destruction:
	QAngleByValue(void) : QAngle() {} 
	QAngleByValue(vec_t X, vec_t Y, vec_t Z) : QAngle( X, Y, Z ) {}
	QAngleByValue(const QAngleByValue& vOther) = default;
};

static_assert(alignof(QAngleByValue) == alignof(QAngle),
              "Need for Vector::operator VectorByValue& {const} support.");

inline void XM_CALLCONV VectorAdd( const QAngle& a, const QAngle& b, QAngle& result )
{
	CHECK_VALID(a);
	CHECK_VALID(b);

	DirectX::XMStoreFloat3
	(
		result.XmBase(),
		DirectX::XMVectorAdd
		(
			DirectX::XMLoadFloat3( a.XmBase() ),
			DirectX::XMLoadFloat3( b.XmBase() )
		)
	);
}

inline void XM_CALLCONV VectorMA( const QAngle &start, float scale, const QAngle &direction, QAngle &dest )
{
	CHECK_VALID(start);
	CHECK_VALID(direction);

	DirectX::XMStoreFloat3
	(
		dest.XmBase(),
		DirectX::XMVectorMultiplyAdd
		(
			DirectX::XMLoadFloat3( direction.XmBase() ),
			DirectX::XMVectorReplicate( scale ),
			DirectX::XMLoadFloat3( start.XmBase() )
		)
	);
}


//-----------------------------------------------------------------------------
// constructors
//-----------------------------------------------------------------------------
inline QAngle::QAngle(vec_t X, vec_t Y, vec_t Z)
	: x{X}, y{Y}, z{Z}
{ 
	CHECK_VALID(*this);
}


//-----------------------------------------------------------------------------
// initialization
//-----------------------------------------------------------------------------
inline void QAngle::Init( vec_t ix, vec_t iy, vec_t iz )
{ 
	x = ix; y = iy; z = iz;
	CHECK_VALID(*this);
}

inline void QAngle::Random( vec_t minVal, vec_t maxVal )
{
	DirectX::XMVECTOR rn = DirectX::XMVectorSet
	(
		(static_cast<float>(rand()) / VALVE_RAND_MAX),
		(static_cast<float>(rand()) / VALVE_RAND_MAX),
		(static_cast<float>(rand()) / VALVE_RAND_MAX),
		0.0f
	);
	DirectX::XMVECTOR mn = DirectX::XMVectorReplicate( minVal );

	DirectX::XMStoreFloat3
	(
		XmBase(),
		DirectX::XMVectorMultiplyAdd
		(
			rn,
			DirectX::XMVectorSubtract( DirectX::XMVectorReplicate( maxVal ), mn ),
			mn
		)
	);

	CHECK_VALID(*this);
}

#ifndef VECTOR_NO_SLOW_OPERATIONS

inline QAngle XM_CALLCONV RandomAngle( float minVal, float maxVal )
{
	QAngle ret;
	ret.Random( minVal, maxVal );
	return ret;
}

#endif


inline RadianEuler::RadianEuler(QAngle const &angles) //-V730
{
	DirectX::XMVECTOR qa = DirectX::XMVectorMultiply
	(
		DirectX::XMLoadFloat3( angles.XmBase() ),
		DirectX::XMVectorReplicate( M_PI_F / 180.0f )
	);

	DirectX::XMStoreFloat3
	(
		XmBase(),
		// qa z -> re x, qa x -> re y, qa y -> re z
		DirectX::XMVectorSwizzle
		<
			DirectX::XM_SWIZZLE_Z,
			DirectX::XM_SWIZZLE_X,
			DirectX::XM_SWIZZLE_Y,
			DirectX::XM_SWIZZLE_W
		>( qa )
	);
}


inline QAngle RadianEuler::ToQAngle() const
{
	DirectX::XMVECTOR re = DirectX::XMVectorMultiply
	(
		DirectX::XMLoadFloat3( XmBase() ),
		DirectX::XMVectorReplicate( 180.0f / M_PI_F )
	);

	QAngle qa;
	DirectX::XMStoreFloat3
	(
		qa.XmBase(),
		// re y -> qa x, re z -> qa y, re x -> qa z
		DirectX::XMVectorSwizzle
		<
			DirectX::XM_SWIZZLE_Y,
			DirectX::XM_SWIZZLE_Z,
			DirectX::XM_SWIZZLE_X,
			DirectX::XM_SWIZZLE_W
		>( re )
	);

	return qa;
}


//-----------------------------------------------------------------------------
// Array access
//-----------------------------------------------------------------------------
inline vec_t& QAngle::operator[](int i)
{
	Assert( (i >= 0) && (i < 3) );
	static_assert(alignof(QAngle) == alignof(vec_t));
	return reinterpret_cast<vec_t*>(this)[i];
}

inline vec_t QAngle::operator[](int i) const
{
	Assert( (i >= 0) && (i < 3) );
	static_assert(alignof(QAngle) == alignof(vec_t));
	return reinterpret_cast<const vec_t*>(this)[i];
}


//-----------------------------------------------------------------------------
// Base address...
//-----------------------------------------------------------------------------
inline vec_t* QAngle::Base()
{
	static_assert(alignof(QAngle) == alignof(vec_t));
	return reinterpret_cast<vec_t*>(this);
}

inline vec_t const* QAngle::Base() const
{
	static_assert(alignof(QAngle) == alignof(vec_t));
	return reinterpret_cast<const vec_t*>(this);
}


//-----------------------------------------------------------------------------
// IsValid?
//-----------------------------------------------------------------------------
inline bool QAngle::IsValid() const
{
	return !DirectX::XMVector3IsNaN( DirectX::XMLoadFloat3( XmBase() ) );
}

//-----------------------------------------------------------------------------
// Invalidate
//-----------------------------------------------------------------------------

inline void QAngle::Invalidate()
{
//#ifdef _DEBUG
//#ifdef VECTOR_PARANOIA
	DirectX::XMStoreFloat3( XmBase(), DirectX::XMVectorSplatQNaN() );
//#endif
//#endif
}

//-----------------------------------------------------------------------------
// comparison
//-----------------------------------------------------------------------------
inline bool QAngle::operator==( const QAngle& src ) const
{
	CHECK_VALID(src);
	CHECK_VALID(*this);

	return DirectX::XMVector3Equal( DirectX::XMLoadFloat3( XmBase() ), DirectX::XMLoadFloat3( src.XmBase() ) );
}

inline bool QAngle::operator!=( const QAngle& src ) const
{
	return !(*this == src);
}


//-----------------------------------------------------------------------------
// Copy
//-----------------------------------------------------------------------------
inline void XM_CALLCONV VectorCopy( const QAngle& src, QAngle& dst )
{
	CHECK_VALID(src);
	dst.x = src.x;
	dst.y = src.y;
	dst.z = src.z;
}


//-----------------------------------------------------------------------------
// standard math operations
//-----------------------------------------------------------------------------
inline QAngle& QAngle::operator+=(const QAngle& v)
{ 
	CHECK_VALID(*this);
	CHECK_VALID(v);

	DirectX::XMStoreFloat3
	(
		XmBase(),
		DirectX::XMVectorAdd
		(
			DirectX::XMLoadFloat3( XmBase() ),
			DirectX::XMLoadFloat3( v.XmBase() )
		)
	);

	return *this;
}

inline QAngle& QAngle::operator-=(const QAngle& v)
{ 
	CHECK_VALID(*this);
	CHECK_VALID(v);

	DirectX::XMStoreFloat3
	(
		XmBase(),
		DirectX::XMVectorSubtract
		(
			DirectX::XMLoadFloat3( XmBase() ),
			DirectX::XMLoadFloat3( v.XmBase() )
		)
	);

	return *this;
}

inline QAngle& QAngle::operator*=(float fl)
{
	DirectX::XMStoreFloat3
	(
		XmBase(),
		DirectX::XMVectorScale
		(
			DirectX::XMLoadFloat3( XmBase() ),
			fl
		)
	);

	CHECK_VALID(*this);
	return *this;
}

inline QAngle& QAngle::operator/=(float fl)
{
	Assert( fl != 0.0f );
	float oofl = 1.0f / fl;

	DirectX::XMStoreFloat3
	(
		XmBase(),
		DirectX::XMVectorScale
		(
			DirectX::XMLoadFloat3( XmBase() ),
			oofl
		)
	);

	CHECK_VALID(*this);
	return *this;
}


//-----------------------------------------------------------------------------
// length
//-----------------------------------------------------------------------------
inline vec_t QAngle::Length() const
{
	CHECK_VALID(*this);

	return DirectX::XMVectorGetX
	(
		DirectX::XMVector3Length( DirectX::XMLoadFloat3( XmBase() ) )
	);
}


inline vec_t QAngle::LengthSqr() const
{
	CHECK_VALID(*this);

	return DirectX::XMVectorGetX
	(
		DirectX::XMVector3LengthSq( DirectX::XMLoadFloat3( XmBase() ) )
	);
}
	

//-----------------------------------------------------------------------------
// Vector equality with tolerance
//-----------------------------------------------------------------------------
inline bool XM_CALLCONV QAnglesAreEqual( const QAngle& src1, const QAngle& src2, float tolerance = 0.0f )
{
	return DirectX::XMVector3NearEqual
	(
		DirectX::XMLoadFloat3( src1.XmBase() ),
		DirectX::XMLoadFloat3( src2.XmBase() ),
		DirectX::XMVectorReplicate( tolerance )
	);
}


//-----------------------------------------------------------------------------
// arithmetic operations (SLOW!!)
//-----------------------------------------------------------------------------
#ifndef VECTOR_NO_SLOW_OPERATIONS

inline QAngle QAngle::operator-() const
{
	QAngle r;
	DirectX::XMStoreFloat3
	(
		r.XmBase(),
		DirectX::XMVectorNegate( DirectX::XMLoadFloat3( XmBase() ) )
	);
	return r;
}

inline QAngle QAngle::operator+(const QAngle& v) const
{
	QAngle r;
	DirectX::XMStoreFloat3
	(
		r.XmBase(),
		DirectX::XMVectorAdd
		(
			DirectX::XMLoadFloat3( XmBase() ),
			DirectX::XMLoadFloat3( v.XmBase() )
		)
	);
	return r;
}

inline QAngle QAngle::operator-(const QAngle& v) const
{
	QAngle r;
	DirectX::XMStoreFloat3
	(
		r.XmBase(),
		DirectX::XMVectorSubtract
		(
			DirectX::XMLoadFloat3( XmBase() ),
			DirectX::XMLoadFloat3( v.XmBase() )
		)
	);
	return r;
}

inline QAngle QAngle::operator*(float fl) const
{
	QAngle r;
	DirectX::XMStoreFloat3
	(
		r.XmBase(),
		DirectX::XMVectorScale
		(
			DirectX::XMLoadFloat3( XmBase() ),
			fl
		)
	);
	return r;
}

inline QAngle QAngle::operator/(float fl) const
{
	QAngle r;
	DirectX::XMStoreFloat3
	(
		r.XmBase(),
		DirectX::XMVectorScale
		(
			DirectX::XMLoadFloat3( XmBase() ),
			1.0f / fl
		)
	);
	return r;
}

inline QAngle operator*(float fl, const QAngle& v)
{
	QAngle ret( v * fl );
	return ret;
}

#endif // VECTOR_NO_SLOW_OPERATIONS


//-----------------------------------------------------------------------------
// NOTE: These are not completely correct.  The representations are not equivalent
// unless the QAngle represents a rotational impulse along a coordinate axis (x,y,z)
inline void XM_CALLCONV QAngleToAngularImpulse( const QAngle &angles, AngularImpulse &impulse )
{
	impulse.x = angles.z;
	impulse.y = angles.x;
	impulse.z = angles.y;
}

inline void XM_CALLCONV AngularImpulseToQAngle( const AngularImpulse &impulse, QAngle &angles )
{
	angles.x = impulse.y;
	angles.y = impulse.z;
	angles.z = impulse.x;
}

#if !defined( _X360 )

// dimhotepus: Too unsafe. Use safer alternatives.
FORCEINLINE vec_t InvRSquared( float const *v ) = delete;
//{
//#if defined(__i386__) || defined(_M_IX86)
//	float sqrlen = v[0]*v[0]+v[1]*v[1]+v[2]*v[2] + 1.0e-10f, result;
//	_mm_store_ss(&result, _mm_rcp_ss( _mm_max_ss( _mm_set_ss(1.0f), _mm_load_ss(&sqrlen) ) ));
//	return result;
//#else
//	return 1.f/fpmax(1.f, v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
//#endif
//}

#if defined(__i386__) || defined(_M_IX86)
// dimhotepus: Unused? Drop.
inline void _SSE_RSqrtInline( float a, float* out ) = delete;
//{
//	__m128  xx = _mm_load_ss( &a );
//	__m128  xr = _mm_rsqrt_ss( xx );
//	__m128  xt;
//	xt = _mm_mul_ss( xr, xr );
//	xt = _mm_mul_ss( xt, xx );
//	xt = _mm_sub_ss( _mm_set_ss(3.f), xt );
//	xt = _mm_mul_ss( xt, _mm_set_ss(0.5f) );
//	xr = _mm_mul_ss( xr, xt );
//	_mm_store_ss( out, xr );
//}
#endif

FORCEINLINE float XM_CALLCONV VectorNormalize( DirectX::XMVECTOR& val )
{
	DirectX::XMVECTOR len = DirectX::XMVector3Length( val );
	float slen = DirectX::XMVectorGetX( len );
	// Prevent division on zero.
	DirectX::XMVECTOR den = DirectX::XMVectorReplicate( 1.f / ( slen + FLT_EPSILON ) );

	val = DirectX::XMVectorMultiply( val, den );
	
	return slen;
}

FORCEINLINE float XM_CALLCONV VectorNormalize( Vector& vec )
{
	CHECK_VALID(vec);

	DirectX::XMVECTOR val = DirectX::XMLoadFloat3( vec.XmBase() );
	float slen = VectorNormalize( val );

	DirectX::XMStoreFloat3( vec.XmBase(), val );

	return slen;
}

FORCEINLINE float XM_CALLCONV VectorNormalize( DirectX::XMFLOAT4 *v )
{
	Assert(v);
	
	// Looks like DirectX::XMVector3Normalize honors w component, which is not we want to.
	DirectX::XMVECTOR val = DirectX::XMVectorSetW
	(
		DirectX::XMLoadFloat4( v ),
		0.0f
	);
	float slen = VectorNormalize( val );

	DirectX::XMStoreFloat4( v, val );
	
	return slen;
}

// FIXME: Obsolete version of VectorNormalize, once we remove all the friggin float*s
// dimhotepus: Too unsafe. Use safer alternatives.
FORCEINLINE float VectorNormalize( float * v ) = delete;
//{
//	return VectorNormalize(*(reinterpret_cast<Vector *>(v)));
//}

FORCEINLINE void XM_CALLCONV VectorNormalizeFast( Vector &vec )
{
	DirectX::XMVECTOR val = DirectX::XMLoadFloat3( vec.XmBase() );
	DirectX::XMVECTOR len = DirectX::XMVector3LengthEst( val );
	// Prevent division on zero.
	DirectX::XMVECTOR den = DirectX::XMVectorReplicate( 1.f / ( DirectX::XMVectorGetX( len ) + FLT_EPSILON ) );

	DirectX::XMStoreFloat3( vec.XmBase(), DirectX::XMVectorMultiply( val, den ) );
}

#endif // _X360


inline vec_t Vector::NormalizeInPlace()
{
	return VectorNormalize( *this );
}

inline Vector Vector::Normalized() const
{
	Vector norm = *this;
	VectorNormalize( norm );
	return norm;
}

inline bool Vector::IsLengthGreaterThan( float val ) const
{
	return LengthSqr() > val*val;
}

inline bool Vector::IsLengthLessThan( float val ) const
{
	return LengthSqr() < val*val;
}

#endif

