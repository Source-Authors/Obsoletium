//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef MATH_LIB_H
#define MATH_LIB_H

#include <DirectXMath.h>
#include <cmath>

#include "minmax.h"
#include "tier0/basetypes.h"
#include "tier0/commonmacros.h"
#include "tier0/dbg.h"

#include "mathlib/vector.h"
#include "mathlib/vector2d.h"
#include "mathlib/math_pfns.h"

// XXX remove me
#undef clamp

// Uncomment this to enable FP exceptions in parts of the code.
// This can help track down FP bugs. However the code is not
// FP exception clean so this not a turnkey operation.
//#define FP_EXCEPTIONS_ENABLED


#ifdef FP_EXCEPTIONS_ENABLED
#include <float.h> // For _clearfp and _controlfp_s
#endif

// FPExceptionDisabler and FPExceptionEnabler taken from my blog post
// at https://randomascii.wordpress.com/2012/04/21/exceptional-floating-point/

// Declare an object of this type in a scope in order to suppress
// all floating-point exceptions temporarily. The old exception
// state will be reset at the end.
class FPExceptionDisabler
{
public:
#ifdef FP_EXCEPTIONS_ENABLED
	FPExceptionDisabler();
	~FPExceptionDisabler();

private:
	unsigned int mOldValues;
#else
	FPExceptionDisabler() = default;
	~FPExceptionDisabler() = default;
#endif

private:
	// Make the copy constructor and assignment operator private
	// and unimplemented to prohibit copying.
	FPExceptionDisabler(const FPExceptionDisabler&) = delete;
	FPExceptionDisabler& operator=(const FPExceptionDisabler&) = delete;
};

// Declare an object of this type in a scope in order to enable a
// specified set of floating-point exceptions temporarily. The old
// exception state will be reset at the end.
// This class can be nested.
class FPExceptionEnabler
{
public:
	// Overflow, divide-by-zero, and invalid-operation are the FP
	// exceptions most frequently associated with bugs.
#ifdef FP_EXCEPTIONS_ENABLED
	FPExceptionEnabler(unsigned int enableBits = _EM_OVERFLOW | _EM_ZERODIVIDE | _EM_INVALID);
	~FPExceptionEnabler();

private:
	unsigned int mOldValues;
#else
	FPExceptionEnabler([[maybe_unused]] unsigned int enableBits = 0)
	{
	}
	~FPExceptionEnabler() = default;
#endif

private:
	// Make the copy constructor and assignment operator private
	// and unimplemented to prohibit copying.
	FPExceptionEnabler(const FPExceptionEnabler&) = delete;
	FPExceptionEnabler& operator=(const FPExceptionEnabler&) = delete;
};



FORCEINLINE float XM_CALLCONV clamp( float val, float minVal, float maxVal )
{
	DirectX::XMVECTOR vres = DirectX::XMVectorClamp
	(
		DirectX::XMLoadFloat( &val ),
		DirectX::XMLoadFloat( &minVal ),
		DirectX::XMLoadFloat( &maxVal )
	);
	return DirectX::XMVectorGetX( vres );
	//val = fpmax(minVal, val);
	//val = fpmin(maxVal, val);
}

//
// Returns a clamped value in the range [min, max].
//
template<typename T>
inline T clamp( T val, T minVal, T maxVal )
{
	return std::clamp( val, minVal, maxVal );
}


// plane_t structure
// !!! if this is changed, it must be changed in asm code too !!!
// FIXME: does the asm code even exist anymore?
// FIXME: this should move to a different file
struct cplane_t
{
	Vector	normal;
	float	dist;
	byte	type;			// for fast side tests
	byte	signbits;		// signx + (signy<<1) + (signz<<1)
	byte	pad[2];

#ifdef VECTOR_NO_SLOW_OPERATIONS
	cplane_t() {}

private:
	// No copy constructors allowed if we're in optimal mode
	cplane_t(const cplane_t& vOther);
#endif
};

// structure offset for asm code
#define CPLANE_NORMAL_X			0
#define CPLANE_NORMAL_Y			4
#define CPLANE_NORMAL_Z			8
#define CPLANE_DIST				12
#define CPLANE_TYPE				16
#define CPLANE_SIGNBITS			17
#define CPLANE_PAD0				18
#define CPLANE_PAD1				19

// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// 3-5 are non-axial planes snapped to the nearest
#define	PLANE_ANYX		3
#define	PLANE_ANYY		4
#define	PLANE_ANYZ		5


//-----------------------------------------------------------------------------
// Frustum plane indices.
// WARNING: there is code that depends on these values
//-----------------------------------------------------------------------------

enum
{
	FRUSTUM_RIGHT		= 0,
	FRUSTUM_LEFT		= 1,
	FRUSTUM_TOP			= 2,
	FRUSTUM_BOTTOM		= 3,
	FRUSTUM_NEARZ		= 4,
	FRUSTUM_FARZ		= 5,
	FRUSTUM_NUMPLANES	= 6
};

extern byte SignbitsForPlane( cplane_t *out );

class Frustum_t
{
public:
	void SetPlane( int i, byte nType, const Vector &vecNormal, float dist )
	{
		m_Plane[i].normal = vecNormal;
		m_Plane[i].dist = dist;
		m_Plane[i].type = nType;
		m_Plane[i].signbits = SignbitsForPlane( &m_Plane[i] );
		m_AbsNormal[i].Init( fabsf(vecNormal.x), fabsf(vecNormal.y), fabsf(vecNormal.z) );
	}

	inline const cplane_t *GetPlane( int i ) const { return &m_Plane[i]; }
	inline const Vector &GetAbsNormal( int i ) const { return m_AbsNormal[i]; }

private:
	cplane_t	m_Plane[FRUSTUM_NUMPLANES];
	Vector		m_AbsNormal[FRUSTUM_NUMPLANES];
};

// Computes Y fov from an X fov and a screen aspect ratio + X from Y
float XM_CALLCONV CalcFovY( float flFovX, float flScreenAspect );
float XM_CALLCONV CalcFovX( float flFovY, float flScreenAspect );

// Generate a frustum based on perspective view parameters
// NOTE: FOV is specified in degrees, as the *full* view angle (not half-angle)
void XM_CALLCONV GeneratePerspectiveFrustum( const Vector& origin, const QAngle &angles, float flZNear, float flZFar, float flFovX, float flAspectRatio, Frustum_t &frustum );
void XM_CALLCONV GeneratePerspectiveFrustum( const Vector& origin, const Vector &forward, const Vector &right, const Vector &up, float flZNear, float flZFar, float flFovX, float flFovY, Frustum_t &frustum );

// Cull the world-space bounding box to the specified frustum.
bool XM_CALLCONV R_CullBox( const Vector& mins, const Vector& maxs, const Frustum_t &frustum );
bool XM_CALLCONV R_CullBoxSkipNear( const Vector& mins, const Vector& maxs, const Frustum_t &frustum );

struct matrix3x4_t
{
	matrix3x4_t() = default;
	matrix3x4_t( 
		float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23 )
	{
		m_flMatVal[0][0] = m00;	m_flMatVal[0][1] = m01; m_flMatVal[0][2] = m02; m_flMatVal[0][3] = m03;
		m_flMatVal[1][0] = m10;	m_flMatVal[1][1] = m11; m_flMatVal[1][2] = m12; m_flMatVal[1][3] = m13;
		m_flMatVal[2][0] = m20;	m_flMatVal[2][1] = m21; m_flMatVal[2][2] = m22; m_flMatVal[2][3] = m23;
	}

	//-----------------------------------------------------------------------------
	// Creates a matrix where the X axis = forward
	// the Y axis = left, and the Z axis = up
	//-----------------------------------------------------------------------------
	void XM_CALLCONV Init( const Vector& xAxis, const Vector& yAxis, const Vector& zAxis, const Vector &vecOrigin )
	{
		m_flMatVal[0][0] = xAxis.x; m_flMatVal[0][1] = yAxis.x; m_flMatVal[0][2] = zAxis.x; m_flMatVal[0][3] = vecOrigin.x;
		m_flMatVal[1][0] = xAxis.y; m_flMatVal[1][1] = yAxis.y; m_flMatVal[1][2] = zAxis.y; m_flMatVal[1][3] = vecOrigin.y;
		m_flMatVal[2][0] = xAxis.z; m_flMatVal[2][1] = yAxis.z; m_flMatVal[2][2] = zAxis.z; m_flMatVal[2][3] = vecOrigin.z;
	}

	//-----------------------------------------------------------------------------
	// Creates a matrix where the X axis = forward
	// the Y axis = left, and the Z axis = up
	//-----------------------------------------------------------------------------
	matrix3x4_t( const Vector& xAxis, const Vector& yAxis, const Vector& zAxis, const Vector &vecOrigin )
	{
		Init( xAxis, yAxis, zAxis, vecOrigin );
	}

	inline void Invalidate()
	{
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				m_flMatVal[i][j] = VEC_T_NAN;
			}
		}
	}

	float * XM_CALLCONV operator[]( int i )				{ Assert(( i >= 0 ) && ( i < 3 )); return m_flMatVal[i]; }
	const float * XM_CALLCONV operator[]( int i ) const	{ Assert(( i >= 0 ) && ( i < 3 )); return m_flMatVal[i]; }
	float * XM_CALLCONV Base()							{ return &m_flMatVal[0][0]; }
	const float * XM_CALLCONV Base() const				{ return &m_flMatVal[0][0]; }

	// dimhotepus: Better DirectX math integration.
	DirectX::XMFLOAT4* XM_CALLCONV XmBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT4) == sizeof(m_flMatVal[0]));
		static_assert(alignof(DirectX::XMFLOAT4) == alignof(decltype(m_flMatVal[0])));
		return reinterpret_cast<DirectX::XMFLOAT4*>(&m_flMatVal[0][0]);
	}
	DirectX::XMFLOAT4 const* XM_CALLCONV XmBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT4) == sizeof(m_flMatVal[0]));
		static_assert(alignof(DirectX::XMFLOAT4) == alignof(decltype(m_flMatVal[0])));
		return reinterpret_cast<DirectX::XMFLOAT4 const*>(&m_flMatVal[0][0]);
	}

	DirectX::XMFLOAT3X4* XM_CALLCONV XmMBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT3X4) == sizeof(m_flMatVal));
		static_assert(alignof(DirectX::XMFLOAT3X4) == alignof(decltype(m_flMatVal)));
		return reinterpret_cast<DirectX::XMFLOAT3X4*>(&m_flMatVal[0][0]);
	}
	DirectX::XMFLOAT3X4 const* XM_CALLCONV XmMBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT3X4) == sizeof(m_flMatVal));
		static_assert(alignof(DirectX::XMFLOAT3X4) == alignof(decltype(m_flMatVal)));
		return reinterpret_cast<DirectX::XMFLOAT3X4 const*>(&m_flMatVal[0][0]);
	}

	float m_flMatVal[3][4];
};


// dimhotepus: tier0/basetypes.h already has these.
//#ifndef M_PI
//constexpr inline double M_PI{3.14159265358979323846};	// matches value in gcc v2 math.h
//#endif
//
//#ifndef M_PI_F
//constexpr inline float M_PI_F{3.14159265358979323846f};	// Shouldn't collide with anything.
//#endif

// NJS: Inlined to prevent floats from being autopromoted to doubles, as with the old system.
#ifndef RAD2DEG
constexpr inline float XM_CALLCONV RAD2DEG(float x) { return x * (180.f / M_PI_F); }
constexpr inline double XM_CALLCONV RAD2DEG(double x) { return x * (180.0 / M_PI); }
#endif

#ifndef DEG2RAD
constexpr inline float XM_CALLCONV DEG2RAD(float x) { return x * (M_PI_F / 180.f); }
constexpr inline double XM_CALLCONV DEG2RAD(double x) { return x * (M_PI / 180.0); }
#endif

// Used to represent sides of things like planes.
#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2
#define SIDE_CROSS  -2      // necessary for polylib.c

#define ON_VIS_EPSILON  0.01    // necessary for vvis (flow.c) -- again look into moving later!
#define	EQUAL_EPSILON	0.001f   // necessary for vbsp (faces.c) -- should look into moving it there?

extern bool s_bMathlibInitialized;

extern  const Vector vec3_origin;
extern  const QAngle vec3_angle;
extern	const Quaternion quat_identity;
extern const Vector vec3_invalid;
extern	const int nanmask;

template<typename T>
inline bool IS_NAN(T x)
{
	return std::isnan(x);
}

FORCEINLINE vec_t XM_CALLCONV DotProduct(const vec_t *v1, const vec_t *v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}
FORCEINLINE void XM_CALLCONV VectorSubtract(const vec_t *a, const vec_t *b, vec_t *c)
{
	c[0]=a[0]-b[0];
	c[1]=a[1]-b[1];
	c[2]=a[2]-b[2];
}
FORCEINLINE void XM_CALLCONV VectorAdd(const vec_t *a, const vec_t *b, vec_t *c)
{
	c[0]=a[0]+b[0];
	c[1]=a[1]+b[1];
	c[2]=a[2]+b[2];
}
FORCEINLINE void XM_CALLCONV VectorCopy(const vec_t *a, vec_t *b)
{
	b[0]=a[0];
	b[1]=a[1];
	b[2]=a[2];
}

// dimhotepus: Too unsafe, Use overloads.
FORCEINLINE void VectorClear(vec_t *a) = delete;
//{
//	a[0]=a[1]=a[2]=0;
//}

// dimhotepus: Too unsafe, Use overloads.
FORCEINLINE float VectorMaximum(const vec_t *v) = delete;
//{
//	return max( v[0], max( v[1], v[2] ) );
//}

FORCEINLINE float XM_CALLCONV VectorMaximum(const Vector& v)
{
	return max( v.x, max( v.y, v.z ) );
}

FORCEINLINE void XM_CALLCONV VectorScale (const float* in, vec_t scale, float* out)
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}


// Cannot be forceinline as they have overloads:
// dimhotepus: Unsafe. Use safe overloads.
inline void VectorFill(vec_t *a, float b) = delete;
//{
//	a[0]=a[1]=a[2]=b;
//}

// dimhotepus: Unsafe. Use safe overloads.
inline void VectorNegate(vec_t *a) = delete;
//{
//	a[0]=-a[0];
//	a[1]=-a[1];
//	a[2]=-a[2];
//}


//#define VectorMaximum(a)		( max( (a)[0], max( (a)[1], (a)[2] ) ) )
#define Vector2Clear(x)			{(x)[0]=(x)[1]=0;}
#define Vector2Negate(x)		{(x)[0]=-((x)[0]);(x)[1]=-((x)[1]);}
#define Vector2Copy(a,b)		{(b)[0]=(a)[0];(b)[1]=(a)[1];}
#define Vector2Subtract(a,b,c)	{(c)[0]=(a)[0]-(b)[0];(c)[1]=(a)[1]-(b)[1];}
#define Vector2Add(a,b,c)		{(c)[0]=(a)[0]+(b)[0];(c)[1]=(a)[1]+(b)[1];}
#define Vector2Scale(a,b,c)		{(c)[0]=(b)*(a)[0];(c)[1]=(b)*(a)[1];}

// NJS: Some functions in VBSP still need to use these for dealing with mixing vec4's and shorts with vec_t's.
// remove when no longer needed.
#define VECTOR_COPY( A, B ) do { (B)[0] = (A)[0]; (B)[1] = (A)[1]; (B)[2]=(A)[2]; } while(0)
#define DOT_PRODUCT( A, B ) ( (A)[0]*(B)[0] + (A)[1]*(B)[1] + (A)[2]*(B)[2] )

// dimhotepus: Too unsafe. Use overloads.
FORCEINLINE void VectorMAInline( const float* start, float scale, const float* direction, float* dest ) = delete;
//{
//	dest[0]=start[0]+direction[0]*scale;
//	dest[1]=start[1]+direction[1]*scale;
//	dest[2]=start[2]+direction[2]*scale;
//}

FORCEINLINE void XM_CALLCONV VectorMAInline( const Vector& start, float scale, const Vector& direction, Vector& dest )
{
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

FORCEINLINE void XM_CALLCONV VectorMA( const Vector& start, float scale, const Vector& direction, Vector& dest )
{
	VectorMAInline(start, scale, direction, dest);
}

// dimhotepus: Unsafe. Use overloads.
FORCEINLINE void VectorMA( const float * start, float scale, const float *direction, float *dest ) = delete;
//{
//	VectorMAInline(start, scale, direction, dest);
//}

// dimhotepus: Too unsafe. Use overloads.
int VectorCompare (const float *v1, const float *v2) = delete;

inline float XM_CALLCONV VectorLength(const float *v)
{
	return FastSqrt( v[0]*v[0] + v[1]*v[1] + v[2]*v[2] + FLT_EPSILON );
}

// dimhotepus: Too unsafe. Use safer alternatives.
void CrossProduct (const float *v1, const float *v2, float *cross) = delete;

// dimhotepus: Too unsafe. Use safer alternatives.
qboolean VectorsEqual( const float *v1, const float *v2 ) = delete;

inline vec_t XM_CALLCONV RoundInt(vec_t in)
{
	return floorf(in + 0.5f);
}

constexpr inline int XM_CALLCONV Q_log2(int val)
{
	int answer=0;
	while (val>>=1)
		answer++;
	return answer;
}

// Math routines done in optimized assembly math package routines
FORCEINLINE void XM_CALLCONV SinCos( float radians, float *sine, float *cosine )
{
	DirectX::XMScalarSinCos( sine, cosine, radians );
}


template<class T>
FORCEINLINE constexpr T Square( T a )
{
	return a * a;
}


// return the smallest power of two >= x.
// returns 0 if x == 0 or x > 0x80000000 (ie numbers that would be negative if x was signed)
// NOTE: the old code took an int, and if you pass in an int of 0x80000000 casted to a uint,
//       you'll get 0x80000000, which is correct for uints, instead of 0, which was correct for ints
FORCEINLINE constexpr uint XM_CALLCONV SmallestPowerOfTwoGreaterOrEqual( uint x )
{
	x -= 1;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

// return the smallest power of two >= x.
// returns 0 if x == 0 or x > 0x80000000 (ie numbers that would be negative if x was signed)
// NOTE: the old code took an int, and if you pass in an int of 0x80000000 casted to a uint,
//       you'll get 0x80000000, which is correct for uints, instead of 0, which was correct for ints
FORCEINLINE constexpr uint XM_CALLCONV SmallestPowerOfTwoGreaterOrEqual( int x )
{
	x -= 1;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

// return the smallest power of two >= x.
// returns 0 if x == 0 or x > 0x8000000000000000 (ie numbers that would be negative if x was signed)
// NOTE: the old code took an int, and if you pass in an int of 0x8000000000000000 casted to a uint64,
//       you'll get 0x8000000000000000, which is correct for uints, instead of 0, which was correct for ints
FORCEINLINE constexpr uint64 XM_CALLCONV SmallestPowerOfTwoGreaterOrEqual( uint64 x )
{
	x -= 1;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> 32;
	return x + 1;
}

// return the smallest power of two >= x.
// returns 0 if x == 0 or x > 0x8000000000000000 (ie numbers that would be negative if x was signed)
// NOTE: the old code took an int, and if you pass in an int of 0x8000000000000000 casted to a uint64,
//       you'll get 0x8000000000000000, which is correct for uints, instead of 0, which was correct for ints
FORCEINLINE constexpr int64 XM_CALLCONV SmallestPowerOfTwoGreaterOrEqual( int64 x )
{
	x -= 1;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> 32;
	return x + 1;
}

// return the largest power of two <= x. Will return 0 if passed 0
FORCEINLINE constexpr uint XM_CALLCONV LargestPowerOfTwoLessThanOrEqual( uint x )
{
	if ( x >= 0x80000000 )
		return 0x80000000;

	return SmallestPowerOfTwoGreaterOrEqual( x + 1 ) >> 1;
}

// return the largest power of two <= x. Will return 0 if passed 0
FORCEINLINE constexpr uint XM_CALLCONV LargestPowerOfTwoLessThanOrEqual( int x )
{
	if ( (uint)x >= 0x80000000 )
		return 0x80000000;

	return SmallestPowerOfTwoGreaterOrEqual( x + 1 ) >> 1;
}

// return the largest power of two <= x. Will return 0 if passed 0
FORCEINLINE constexpr uint64 XM_CALLCONV LargestPowerOfTwoLessThanOrEqual( uint64 x )
{
	if ( x >= 0x8000000000000000 )
		return 0x8000000000000000;

	return SmallestPowerOfTwoGreaterOrEqual( x + 1 ) >> 1;
}

// return the largest power of two <= x. Will return 0 if passed 0
FORCEINLINE constexpr int64 XM_CALLCONV LargestPowerOfTwoLessThanOrEqual( int64 x )
{
	if ( (uint64)x >= 0x8000000000000000 )
		return 0x8000000000000000;

	return SmallestPowerOfTwoGreaterOrEqual( x + 1 ) >> 1;
}

// Math routines for optimizing division
void XM_CALLCONV FloorDivMod (double numer, double denom, int *quotient, int *rem);
int XM_CALLCONV GreatestCommonDivisor (int i1, int i2);

// Test for FPU denormal mode
bool XM_CALLCONV IsDenormal( float val );

// MOVEMENT INFO
enum
{
	PITCH = 0,	// up / down
	YAW,		// left / right
	ROLL		// fall over
};

void XM_CALLCONV MatrixAngles( const matrix3x4_t & matrix, float *angles ); // !!!!
void XM_CALLCONV MatrixVectors( const matrix3x4_t &matrix, Vector* pForward, Vector *pRight, Vector *pUp );
void XM_CALLCONV VectorTransform (const float *in1, const matrix3x4_t & in2, float *out);
void XM_CALLCONV VectorITransform (const float *in1, const matrix3x4_t & in2, float *out);
void XM_CALLCONV VectorRotate( const float *in1, const matrix3x4_t & in2, float *out);
void XM_CALLCONV VectorRotate( const Vector &in1, const QAngle &in2, Vector &out );
void XM_CALLCONV VectorRotate( const Vector &in1, const Quaternion &in2, Vector &out );
void XM_CALLCONV VectorIRotate( const float *in1, const matrix3x4_t & in2, float *out);

#ifndef VECTOR_NO_SLOW_OPERATIONS

QAngle XM_CALLCONV TransformAnglesToLocalSpace( const QAngle &angles, const matrix3x4_t &parentMatrix );
QAngle XM_CALLCONV TransformAnglesToWorldSpace( const QAngle &angles, const matrix3x4_t &parentMatrix );

#endif

void XM_CALLCONV MatrixInitialize( matrix3x4_t &mat, const Vector &vecOrigin, const Vector &vecXAxis, const Vector &vecYAxis, const Vector &vecZAxis );
void XM_CALLCONV MatrixCopy( const matrix3x4_t &in, matrix3x4_t &out );
void XM_CALLCONV MatrixInvert( const matrix3x4_t &in, matrix3x4_t &out );

// Matrix equality test
bool XM_CALLCONV MatricesAreEqual( const matrix3x4_t &src1, const matrix3x4_t &src2, float flTolerance = 1e-5f );

void XM_CALLCONV MatrixGetColumn( const matrix3x4_t &in, int column, Vector &out );
void XM_CALLCONV MatrixSetColumn( const Vector &in, int column, matrix3x4_t &out );

inline void XM_CALLCONV MatrixGetTranslation( const matrix3x4_t &in, Vector &out )
{
	MatrixGetColumn ( in, 3, out );
}

inline void XM_CALLCONV MatrixSetTranslation( const Vector &in, matrix3x4_t &out )
{
	MatrixSetColumn ( in, 3, out );
}

void XM_CALLCONV MatrixScaleBy ( const float flScale, matrix3x4_t &out );
void XM_CALLCONV MatrixScaleByZero ( matrix3x4_t &out );

//void DecomposeRotation( const matrix3x4_t &mat, float *out );
void XM_CALLCONV ConcatRotations (const matrix3x4_t &in1, const matrix3x4_t &in2, matrix3x4_t &out);
void XM_CALLCONV ConcatTransforms (const matrix3x4_t &in1, const matrix3x4_t &in2, matrix3x4_t &out);

// For identical interface w/ VMatrix
inline void XM_CALLCONV MatrixMultiply ( const matrix3x4_t &in1, const matrix3x4_t &in2, matrix3x4_t &out )
{
	ConcatTransforms( in1, in2, out );
}

void XM_CALLCONV QuaternionSlerp( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt );
void XM_CALLCONV QuaternionSlerpNoAlign( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt );
void XM_CALLCONV QuaternionBlend( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt );
void XM_CALLCONV QuaternionBlendNoAlign( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt );
void XM_CALLCONV QuaternionIdentityBlend( const Quaternion &p, float t, Quaternion &qt );
float XM_CALLCONV QuaternionAngleDiff( const Quaternion &p, const Quaternion &q );
void XM_CALLCONV QuaternionScale( const Quaternion &p, float t, Quaternion &q );
void XM_CALLCONV QuaternionAlign( const Quaternion &p, const Quaternion &q, Quaternion &qt );
float XM_CALLCONV QuaternionDotProduct( const Quaternion &p, const Quaternion &q );
void XM_CALLCONV QuaternionConjugate( const Quaternion &p, Quaternion &q );
void XM_CALLCONV QuaternionInvert( const Quaternion &p, Quaternion &q );
float XM_CALLCONV QuaternionNormalize(Quaternion &q);
// dimhotepus: QuaternionNormalize without length.
void XM_CALLCONV QuaternionNormalize2( Quaternion &q );
void XM_CALLCONV QuaternionAdd( const Quaternion &p, const Quaternion &q, Quaternion &qt );
void XM_CALLCONV QuaternionMult( const Quaternion &p, const Quaternion &q, Quaternion &qt );
void XM_CALLCONV QuaternionMatrix( const Quaternion &q, matrix3x4_t &matrix );
void XM_CALLCONV QuaternionMatrix( const Quaternion &q, const Vector &pos, matrix3x4_t &matrix );
void XM_CALLCONV QuaternionAngles( const Quaternion &q, QAngle &angles );
void XM_CALLCONV AngleQuaternion( const QAngle& angles, Quaternion &qt );
void XM_CALLCONV QuaternionAngles(const Quaternion &q, RadianEuler &angles);
void XM_CALLCONV AngleQuaternion(RadianEuler const &angles, Quaternion &qt);
void XM_CALLCONV QuaternionAxisAngle( const Quaternion &q, Vector &axis, float &angle );
void XM_CALLCONV AxisAngleQuaternion( const Vector &axis, float angle, Quaternion &q );
void XM_CALLCONV BasisToQuaternion( const Vector &vecForward, const Vector &vecRight, const Vector &vecUp, Quaternion &q );
void XM_CALLCONV MatrixQuaternion( const matrix3x4_t &mat, Quaternion &q );

// A couple methods to find the dot product of a vector with a matrix row or column...
inline float XM_CALLCONV MatrixRowDotProduct( const matrix3x4_t &in1, int row, const Vector& in2 )
{
	Assert( (row >= 0) && (row < 3) );
	return DotProduct( in1[row], in2.Base() ); 
}

inline float XM_CALLCONV MatrixColumnDotProduct( const matrix3x4_t &in1, int col, const Vector& in2 )
{
	Assert( (col >= 0) && (col < 4) );
	return in1[0][col] * in2[0] + in1[1][col] * in2[1] + in1[2][col] * in2[2]; 
}

// dimhotepus: Too unsafe. Use safer overload.
int __cdecl BoxOnPlaneSide (const float *emins, const float *emaxs, const cplane_t *plane) = delete;

inline float XM_CALLCONV anglemod(float a)
{
	a = (360.f/65536) * ((int)(a*(65536.f/360.0f)) & 65535);
	return a;
}

// Remap a value in the range [A,B] to [C,D].
inline float XM_CALLCONV RemapVal( float val, float A, float B, float C, float D)
{
	if ( A == B )
		return val >= B ? D : C;
	return C + (D - C) * (val - A) / (B - A);
}

inline float XM_CALLCONV RemapValClamped( float val, float A, float B, float C, float D)
{
	if ( A == B )
		return val >= B ? D : C;
	float cVal = (val - A) / (B - A);
	cVal = clamp( cVal, 0.0f, 1.0f );

	return C + (D - C) * cVal;
}

// Returns A + (B-A)*flPercent.
// float Lerp( float flPercent, float A, float B );
template <class T>
FORCEINLINE T Lerp( float flPercent, T const &A, T const &B )
{
	return A + (B - A) * flPercent;
}

FORCEINLINE float XM_CALLCONV Sqr( float f )
{
	return f*f;
}

// 5-argument floating point linear interpolation.
// FLerp(f1,f2,i1,i2,x)=
//    f1 at x=i1
//    f2 at x=i2
//   smooth lerp between f1 and f2 at x>i1 and x<i2
//   extrapolation for x<i1 or x>i2
//
//   If you know a function f(x)'s value (f1) at position i1, and its value (f2) at position i2,
//   the function can be linearly interpolated with FLerp(f1,f2,i1,i2,x)
//    i2=i1 will cause a divide by zero.
static inline float XM_CALLCONV FLerp(float f1, float f2, float i1, float i2, float x)
{
  return f1+(f2-f1)*(x-i1)/(i2-i1);
}


#ifndef VECTOR_NO_SLOW_OPERATIONS

// YWB:  Specialization for interpolating euler angles via quaternions...
template<> FORCEINLINE QAngle Lerp<QAngle>( float flPercent, const QAngle& q1, const QAngle& q2 )
{
	// Avoid precision errors
	if ( q1 == q2 )
		return q1;

	Quaternion src, dest;

	// Convert to quaternions
	AngleQuaternion( q1, src );
	AngleQuaternion( q2, dest );

	Quaternion result;

	// Slerp
	QuaternionSlerp( src, dest, flPercent, result );

	// Convert to euler
	QAngle output;
	QuaternionAngles( result, output );
	return output;
}

#else

#pragma error

// NOTE NOTE: I haven't tested this!! It may not work! Check out interpolatedvar.cpp in the client dll to try it
template<> FORCEINLINE QAngleByValue Lerp<QAngleByValue>( float flPercent, const QAngleByValue& q1, const QAngleByValue& q2 )
{
	// Avoid precision errors
	if ( q1 == q2 )
		return q1;

	Quaternion src, dest;

	// Convert to quaternions
	AngleQuaternion( q1, src );
	AngleQuaternion( q2, dest );

	Quaternion result;

	// Slerp
	QuaternionSlerp( src, dest, flPercent, result );

	// Convert to euler
	QAngleByValue output;
	QuaternionAngles( result, output );
	return output;
}

#endif // VECTOR_NO_SLOW_OPERATIONS


/// Same as swap(), but won't cause problems with std::swap
template <class T> 
FORCEINLINE void V_swap( T& x, T& y )
{
	T temp = x;
	x = y;
	y = temp;
}

template <class T> FORCEINLINE T AVG(T a, T b)
{
	return (a+b)/2;
}

// number of elements in an array of static size
// dimhotepus: Deprecated, use std::size / std::ssize
#define NELEMS(x) ARRAYSIZE(x)

// XYZ macro, for printf type functions - ex printf("%f %f %f",XYZ(myvector));
#define XYZ(v) (v).x,(v).y,(v).z


inline float XM_CALLCONV Sign( float x )
{
	return (x < 0.0f) ? -1.0f : 1.0f;
}

inline int XM_CALLCONV Sign( int x )
{
	return (x < 0) ? -1 : 1;
}

//
// Clamps the input integer to the given array bounds.
// Equivalent to the following, but without using any branches:
//
// if( n < 0 ) return 0;
// else if ( n > maxindex ) return maxindex;
// else return n;
//
// This is not always a clear performance win, but when you have situations where a clamped 
// value is thrashing against a boundary this is a big win. (ie, valid, invalid, valid, invalid, ...)
//
// Note: This code has been run against all possible integers.
//
inline int XM_CALLCONV ClampArrayBounds( int n, unsigned maxindex )
{
	// mask is 0 if less than 4096, 0xFFFFFFFF if greater than
	unsigned int inrangemask = 0xFFFFFFFF + (((unsigned) n) > maxindex );
	unsigned int lessthan0mask = 0xFFFFFFFF + ( n >= 0 );
	
	// If the result was valid, set the result, (otherwise sets zero)
	int result = (inrangemask & n);

	// if the result was out of range or zero.
	result |= ((~inrangemask) & (~lessthan0mask)) & maxindex;

	return result;
}


#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide( (emins), (emaxs), (p)))

//-----------------------------------------------------------------------------
// FIXME: Vector versions.... the float versions will go away hopefully soon!
//-----------------------------------------------------------------------------

void XM_CALLCONV AngleVectors (const QAngle& angles, Vector *forward);
void XM_CALLCONV AngleVectors (const QAngle& angles, Vector *forward, Vector *right, Vector *up);
void XM_CALLCONV AngleVectorsTranspose (const QAngle& angles, Vector *forward, Vector *right, Vector *up);
void XM_CALLCONV AngleMatrix (const QAngle &angles, matrix3x4_t &mat );
void XM_CALLCONV AngleMatrix( const QAngle &angles, const Vector &position, matrix3x4_t &mat );
void XM_CALLCONV AngleMatrix (const RadianEuler &angles, matrix3x4_t &mat );
void XM_CALLCONV AngleMatrix( RadianEuler const &angles, const Vector &position, matrix3x4_t &mat );
void XM_CALLCONV AngleIMatrix (const QAngle &angles, matrix3x4_t &mat );
void XM_CALLCONV AngleIMatrix (const QAngle &angles, const Vector &position, matrix3x4_t &mat );
void XM_CALLCONV AngleIMatrix (const RadianEuler &angles, matrix3x4_t &mat );
void XM_CALLCONV VectorAngles( const Vector &forward, QAngle &angles );
void XM_CALLCONV VectorAngles( const Vector &forward, const Vector &pseudoup, QAngle &angles );
void XM_CALLCONV VectorMatrix( const Vector &forward, matrix3x4_t &mat );
void XM_CALLCONV VectorVectors( const Vector &forward, Vector &right, Vector &up );
void XM_CALLCONV SetIdentityMatrix( matrix3x4_t &mat );
void XM_CALLCONV SetScaleMatrix( float x, float y, float z, matrix3x4_t &dst );
void XM_CALLCONV MatrixBuildRotationAboutAxis( const Vector &vAxisOfRot, float angleDegrees, matrix3x4_t &dst );

inline void XM_CALLCONV SetScaleMatrix( float flScale, matrix3x4_t &dst )
{
	SetScaleMatrix( flScale, flScale, flScale, dst );
}

inline void XM_CALLCONV SetScaleMatrix( const Vector& scale, matrix3x4_t &dst )
{
	SetScaleMatrix( scale.x, scale.y, scale.z, dst );
}

// Computes the inverse transpose
void XM_CALLCONV MatrixTranspose( matrix3x4_t& mat );
void XM_CALLCONV MatrixTranspose( const matrix3x4_t& src, matrix3x4_t& dst );
void XM_CALLCONV MatrixInverseTranspose( const matrix3x4_t& src, matrix3x4_t& dst );

inline void XM_CALLCONV PositionMatrix( const Vector &position, matrix3x4_t &mat )
{
	MatrixSetColumn( position, 3, mat );
}

inline void XM_CALLCONV MatrixPosition( const matrix3x4_t &matrix, Vector &position )
{
	MatrixGetColumn( matrix, 3, position );
}

void XM_CALLCONV VectorRotate( const Vector& in1, const matrix3x4_t &in2, Vector &out );

void XM_CALLCONV VectorIRotate( Vector in1, const matrix3x4_t &in2, Vector &out );

inline DirectX::XMVECTOR XM_CALLCONV VectorIRotate( DirectX::FXMVECTOR in1, const matrix3x4_t &in2 )
{
	return DirectX::XMVectorSet
	(
		DirectX::XMVectorGetX( DirectX::XMVectorSum( DirectX::XMVectorMultiply( in1, DirectX::XMVectorSet( in2[0][0], in2[1][0], in2[2][0], 0.0F ) ) ) ),
		DirectX::XMVectorGetX( DirectX::XMVectorSum( DirectX::XMVectorMultiply( in1, DirectX::XMVectorSet( in2[0][1], in2[1][1], in2[2][1], 0.0F ) ) ) ),
		DirectX::XMVectorGetX( DirectX::XMVectorSum( DirectX::XMVectorMultiply( in1, DirectX::XMVectorSet( in2[0][2], in2[1][2], in2[2][2], 0.0F ) ) ) ),
		0.0F
	);
}

inline void XM_CALLCONV MatrixAngles( const matrix3x4_t &matrix, QAngle &angles )
{
	MatrixAngles( matrix, &angles.x );
}

inline void XM_CALLCONV MatrixAngles( const matrix3x4_t &matrix, QAngle &angles, Vector &position )
{
	MatrixAngles( matrix, angles );
	MatrixPosition( matrix, position );
}

inline void XM_CALLCONV MatrixAngles( const matrix3x4_t &matrix, RadianEuler &angles )
{
	MatrixAngles( matrix, &angles.x );

	angles.Init( DEG2RAD( angles.z ), DEG2RAD( angles.x ), DEG2RAD( angles.y ) );
}

void XM_CALLCONV MatrixAngles( const matrix3x4_t &mat, RadianEuler &angles, Vector &position );

void XM_CALLCONV MatrixAngles( const matrix3x4_t &mat, Quaternion &q, Vector &position );

inline int XM_CALLCONV VectorCompare (const Vector& v1, const Vector& v2)
{
	return v1 == v2;
}

void XM_CALLCONV VectorTransform (const Vector& in1, const matrix3x4_t &in2, Vector &out);

void XM_CALLCONV VectorITransform (const Vector& in1, const matrix3x4_t &in2, Vector &out);

/*
inline void DecomposeRotation( const matrix3x4_t &mat, Vector &out )
{
	DecomposeRotation( mat, &out.x );
}
*/

int XM_CALLCONV BoxOnPlaneSide ( Vector emins, Vector emaxs, const cplane_t *plane );

inline void XM_CALLCONV VectorFill(Vector& a, float b)
{
	a[0]=a[1]=a[2]=b;
}

inline void XM_CALLCONV VectorNegate(Vector& a)
{
	a[0] = -a[0];
	a[1] = -a[1];
	a[2] = -a[2];
}

inline vec_t XM_CALLCONV VectorAvg(Vector& a)
{
	return ( a[0] + a[1] + a[2] ) / 3;
}

//-----------------------------------------------------------------------------
// Box/plane test (slow version)
//-----------------------------------------------------------------------------
inline int XM_CALLCONV BoxOnPlaneSide2 (const Vector& emins, const Vector& emaxs, const cplane_t *p, float tolerance = 0.f )
{
	Vector	corners[2];

	if (p->normal[0] < 0)
	{
		corners[0][0] = emins[0];
		corners[1][0] = emaxs[0];
	}
	else
	{
		corners[1][0] = emins[0];
		corners[0][0] = emaxs[0];
	}

	if (p->normal[1] < 0)
	{
		corners[0][1] = emins[1];
		corners[1][1] = emaxs[1];
	}
	else
	{
		corners[1][1] = emins[1];
		corners[0][1] = emaxs[1];
	}

	if (p->normal[2] < 0)
	{
		corners[0][2] = emins[2];
		corners[1][2] = emaxs[2];
	}
	else
	{
		corners[1][2] = emins[2];
		corners[0][2] = emaxs[2];
	}

	int sides = 0;

	float dist1 = DotProduct (p->normal, corners[0]) - p->dist;
	if (dist1 >= tolerance)
		sides = 1;

	float dist2 = DotProduct (p->normal, corners[1]) - p->dist;
	if (dist2 < -tolerance)
		sides |= 2;

	return sides;
}

//-----------------------------------------------------------------------------
// Helpers for bounding box construction
//-----------------------------------------------------------------------------

void XM_CALLCONV ClearBounds (Vector& mins, Vector& maxs);
void XM_CALLCONV AddPointToBounds (const Vector& v, Vector& mins, Vector& maxs);

//
// COLORSPACE/GAMMA CONVERSION STUFF
//
void XM_CALLCONV BuildGammaTable( float gamma, float texGamma, float brightness, int overbright );

// convert texture to linear 0..1 value
inline float XM_CALLCONV TexLightToLinear( int c, int exponent )
{
	extern ALIGN128 float power2_n[256]; 
	Assert( exponent >= -128 && exponent <= 127 );
	return ( float )c * power2_n[exponent+128];
}


// convert texture to linear 0..1 value
int XM_CALLCONV LinearToTexture( float f );
// converts 0..1 linear value to screen gamma (0..255)
int XM_CALLCONV LinearToScreenGamma( float f );
float XM_CALLCONV TextureToLinear( int c );

// compressed color format 
struct ColorRGBExp32
{
	byte r, g, b;
	signed char exponent;
};

void XM_CALLCONV ColorRGBExp32ToVector( const ColorRGBExp32& in, Vector& out );
void XM_CALLCONV VectorToColorRGBExp32( const Vector& v, ColorRGBExp32 &c );

// solve for "x" where "a x^2 + b x + c = 0", return true if solution exists
bool XM_CALLCONV SolveQuadratic( float a, float b, float c, float &root1, float &root2 );

// solves for "a, b, c" where "a x^2 + b x + c = y", return true if solution exists
bool XM_CALLCONV SolveInverseQuadratic( float x1, float y1, float x2, float y2, float x3, float y3, float &a, float &b, float &c );

// solves for a,b,c specified as above, except that it always creates a monotonically increasing or
// decreasing curve if the data is monotonically increasing or decreasing. In order to enforce the
// monoticity condition, it is possible that the resulting quadratic will only approximate the data
// instead of interpolating it. This code is not especially fast.
bool XM_CALLCONV SolveInverseQuadraticMonotonic( float x1, float y1, float x2, float y2, 
									 float x3, float y3, float &a, float &b, float &c );




// solves for "a, b, c" where "1/(a x^2 + b x + c ) = y", return true if solution exists
bool XM_CALLCONV SolveInverseReciprocalQuadratic( float x1, float y1, float x2, float y2, float x3, float y3, float &a, float &b, float &c );

// rotate a vector around the Z axis (YAW)
void XM_CALLCONV VectorYawRotate( const Vector& in, float flYaw, Vector &out);


// Bias takes an X value between 0 and 1 and returns another value between 0 and 1
// The curve is biased towards 0 or 1 based on biasAmt, which is between 0 and 1.
// Lower values of biasAmt bias the curve towards 0 and higher values bias it towards 1.
//
// For example, with biasAmt = 0.2, the curve looks like this:
//
// 1
// |				  *
// |				  *
// |			     *
// |			   **
// |			 **
// |	  	 ****
// |*********
// |___________________
// 0                   1
//
//
// With biasAmt = 0.8, the curve looks like this:
//
// 1
// | 	**************
// |  **
// | * 
// | *
// |* 
// |* 
// |*  
// |___________________
// 0                   1
//
// With a biasAmt of 0.5, Bias returns X.
float XM_CALLCONV Bias( float x, float biasAmt );


// Gain is similar to Bias, but biasAmt biases towards or away from 0.5.
// Lower bias values bias towards 0.5 and higher bias values bias away from it.
//
// For example, with biasAmt = 0.2, the curve looks like this:
//
// 1
// | 				  *
// | 				 *
// | 				**
// |  ***************
// | **
// | *
// |*
// |___________________
// 0                   1
//
//
// With biasAmt = 0.8, the curve looks like this:
//
// 1
// |  		    *****
// |  		 ***
// |  		*
// | 		*
// | 		*
// |   	 ***
// |*****
// |___________________
// 0                   1
float XM_CALLCONV Gain( float x, float biasAmt );


// SmoothCurve maps a 0-1 value into another 0-1 value based on a cosine wave
// where the derivatives of the function at 0 and 1 (and 0.5) are 0. This is useful for
// any fadein/fadeout effect where it should start and end smoothly.
//
// The curve looks like this:
//
// 1
// |  		**
// | 	   *  *
// | 	  *	   *
// | 	  *	   *
// | 	 *		*
// |   **		 **
// |***			   ***
// |___________________
// 0                   1
//
float XM_CALLCONV SmoothCurve( float x );


// This works like SmoothCurve, with two changes:
//
// 1. Instead of the curve peaking at 0.5, it will peak at flPeakPos.
//    (So if you specify flPeakPos=0.2, then the peak will slide to the left).
//
// 2. flPeakSharpness is a 0-1 value controlling the sharpness of the peak.
//    Low values blunt the peak and high values sharpen the peak.
float XM_CALLCONV SmoothCurve_Tweak( float x, float flPeakPos=0.5, float flPeakSharpness=0.5 );


//float ExponentialDecay( float halflife, float dt );
//float ExponentialDecay( float decayTo, float decayTime, float dt );

// halflife is time for value to reach 50%
inline float XM_CALLCONV ExponentialDecay( float halflife, float dt )
{
	// log(0.5) == -0.69314718055994530941723212145818
	return expf( -0.69314718f / halflife * dt);
}

// decayTo is factor the value should decay to in decayTime
inline float XM_CALLCONV ExponentialDecay(float decayTo, float decayTime,
                                          float dt) {
	return expf( logf( decayTo ) / decayTime * dt);
}

// Get the integrated distanced traveled
// decayTo is factor the value should decay to in decayTime
// dt is the time relative to the last velocity update
inline float XM_CALLCONV ExponentialDecayIntegral( float decayTo, float decayTime, float dt  )
{
	return (powf( decayTo, dt / decayTime) * decayTime - decayTime) / logf( decayTo );
}

// hermite basis function for smooth interpolation
// Similar to Gain() above, but very cheap to call
// value should be between 0 & 1 inclusive
inline float XM_CALLCONV SimpleSpline( float value )
{
	float valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return valueSquared * (3 - 2 * value);
}

// remaps a value in [startInterval, startInterval+rangeInterval] from linear to
// spline using SimpleSpline
inline float XM_CALLCONV SimpleSplineRemapVal( float val, float A, float B, float C, float D)
{
	if ( A == B )
		return val >= B ? D : C;
	float cVal = (val - A) / (B - A);
	return C + (D - C) * SimpleSpline( cVal );
}

// remaps a value in [startInterval, startInterval+rangeInterval] from linear to
// spline using SimpleSpline
inline float XM_CALLCONV SimpleSplineRemapValClamped( float val, float A, float B, float C, float D )
{
	if ( A == B )
		return val >= B ? D : C;
	float cVal = (val - A) / (B - A);
	cVal = clamp( cVal, 0.0f, 1.0f );
	return C + (D - C) * SimpleSpline( cVal );
}

FORCEINLINE int XM_CALLCONV RoundFloatToInt(float f)
{
#if defined(_XM_SSE_INTRINSICS_)
	return _mm_cvtss_si32(_mm_load_ss(&f));
#else
#error "Please define your platform"
#endif
}

FORCEINLINE unsigned char XM_CALLCONV RoundFloatToByte(float f)
{
	int nResult = RoundFloatToInt(f);
#ifdef Assert
	Assert( (nResult & ~0xFF) == 0 );
#endif
	return (unsigned char) nResult;
}

FORCEINLINE unsigned long XM_CALLCONV RoundFloatToUnsignedLong(float f)
{
#if defined( _X360 )
#ifdef Assert
	Assert( IsFPUControlWordSet() );
#endif
	union
	{
		double flResult;
		int pIntResult[2];
		unsigned long pResult[2];
	};
	flResult = __fctiw( f );
	Assert( pIntResult[1] >= 0 );
	return pResult[1];
#else  // !X360
	
#if defined( PLATFORM_WINDOWS_PC64 )
	uint nRet = ( uint ) f;
	if ( nRet & 1 )
	{
		if ( ( f - floor( f ) >= 0.5 ) )
		{
			nRet++;
		}
	}
	else
	{
		if ( ( f - floor( f ) > 0.5 ) )
		{
			nRet++;
		}
	}
	return nRet;
#else // PLATFORM_WINDOWS_PC64
	alignas(unsigned long) unsigned char nResult[8];

	#if defined( _WIN32 )
		__asm
		{
			fld f
			fistp       qword ptr nResult
		}
	#elif POSIX
		__asm __volatile__ (
			"fistpl %0;": "=m" (nResult): "t" (f) : "st"
		);
	#endif

	return *((unsigned long*)nResult);
#endif // PLATFORM_WINDOWS_PC64
#endif // !X360
}

FORCEINLINE bool XM_CALLCONV IsIntegralValue( float flValue, float flTolerance = 0.001f )
{
	return fabsf( RoundFloatToInt( flValue ) - flValue ) < flTolerance;
}

// Fast, accurate ftol:
FORCEINLINE int XM_CALLCONV Float2Int( float a )
{
#if defined( _X360 )
	union
	{
		double flResult;
		int pResult[2];
	};
	flResult = __fctiwz( a );
	return pResult[1];
#else  // !X360
	// Rely on compiler to generate CVTTSS2SI on x86
	return (int) a;
#endif
}

// Over 15x faster than: (int)floor(value)
inline int XM_CALLCONV Floor2Int( float a )
{
	int RetVal;

#if defined(_XM_SSE_INTRINSICS_)
	// Convert to int and back, compare, subtract one if too big
	__m128 a128 = _mm_set_ss(a);
	RetVal = _mm_cvtss_si32(a128);
    __m128 rounded128 = _mm_cvt_si2ss(_mm_setzero_ps(), RetVal);
	RetVal -= _mm_comigt_ss( rounded128, a128 );
#else
	RetVal = static_cast<int>( floor(a) );
#endif
	return RetVal;
}

//-----------------------------------------------------------------------------
// Fast color conversion from float to unsigned char
//-----------------------------------------------------------------------------
// dimhotepus: Use unsigned char as return type.
FORCEINLINE unsigned char XM_CALLCONV FastFToC( float c )
{
#if defined( __i386__ )
	// IEEE float bit manipulation works for values between [0, 1<<23)
	union { float f; int i; } convert = { c*255.0f + (float)(1<<23) };
	return convert.i & 255;
#else
	// consoles CPUs suffer from load-hit-store penalty
	return static_cast<unsigned char>(Float2Int( c * 255.0f ));
#endif
}

//-----------------------------------------------------------------------------
// Fast conversion from float to integer with magnitude less than 2**22
//-----------------------------------------------------------------------------
FORCEINLINE int XM_CALLCONV FastFloatToSmallInt( float c )
{
#if defined( __i386__ )
	// IEEE float bit manipulation works for values between [-1<<22, 1<<22)
	union { float f; int i; } convert = { c + (float)(3<<22) };
	return (convert.i & ((1<<23)-1)) - (1<<22);
#else
	// consoles CPUs suffer from load-hit-store penalty
	return Float2Int( c );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Bound input float to .001 (millisecond) boundary
// Input  : in - 
// Output : inline float
//-----------------------------------------------------------------------------
inline float XM_CALLCONV ClampToMsec( float in )
{
	int msec = Floor2Int( in * 1000.0f + 0.5f );
	return 0.001f * msec;
}

// Over 15x faster than: (int)ceil(value)
inline int XM_CALLCONV Ceil2Int( float a )
{
   int RetVal;
#if defined(_XM_SSE_INTRINSICS_)
   // Convert to int and back, compare, add one if too small
   __m128 a128 = _mm_load_ss(&a);
   RetVal = _mm_cvtss_si32(a128);
   __m128 rounded128 = _mm_cvt_si2ss(_mm_setzero_ps(), RetVal);
   RetVal += _mm_comilt_ss( rounded128, a128 );
#else
   RetVal = static_cast<int>( ceil(a) );
#endif
	return RetVal;
}


// Regular signed area of triangle
#define TriArea2D( A, B, C ) \
	( 0.5f * ( ( B.x - A.x ) * ( C.y - A.y ) - ( B.y - A.y ) * ( C.x - A.x ) ) )

// This version doesn't premultiply by 0.5f, so it's the area of the rectangle instead
#define TriArea2DTimesTwo( A, B, C ) \
	( ( ( B.x - A.x ) * ( C.y - A.y ) - ( B.y - A.y ) * ( C.x - A.x ) ) )


// Get the barycentric coordinates of "pt" in triangle [A,B,C].
inline void XM_CALLCONV GetBarycentricCoords2D( 
	Vector2D const &A,
	Vector2D const &B,
	Vector2D const &C,
	Vector2D const &pt,
	float bcCoords[3] )
{
	// Note, because to top and bottom are both x2, the issue washes out in the composite
	float invTriArea = 1.0f / TriArea2DTimesTwo( A, B, C );

	// NOTE: We assume here that the lightmap coordinate vertices go counterclockwise.
	// If not, TriArea2D() is negated so this works out right.
	bcCoords[0] = TriArea2DTimesTwo( B, C, pt ) * invTriArea;
	bcCoords[1] = TriArea2DTimesTwo( C, A, pt ) * invTriArea;
	bcCoords[2] = TriArea2DTimesTwo( A, B, pt ) * invTriArea;
}


// Return true of the sphere might touch the box (the sphere is actually treated
// like a box itself, so this may return true if the sphere's bounding box touches
// a corner of the box but the sphere itself doesn't).
inline bool XM_CALLCONV QuickBoxSphereTest( 
	const Vector& vOrigin,
	float flRadius,
	const Vector& bbMin,
	const Vector& bbMax )
{
	return vOrigin.x - flRadius < bbMax.x && vOrigin.x + flRadius > bbMin.x &&
		vOrigin.y - flRadius < bbMax.y && vOrigin.y + flRadius > bbMin.y && 
		vOrigin.z - flRadius < bbMax.z && vOrigin.z + flRadius > bbMin.z;
}


// Return true of the boxes intersect (but not if they just touch).
inline bool XM_CALLCONV QuickBoxIntersectTest( 
	const Vector& vBox1Min,
	const Vector& vBox1Max,
	const Vector& vBox2Min,
	const Vector& vBox2Max )
{
	return 
		vBox1Min.x < vBox2Max.x && vBox1Max.x > vBox2Min.x &&
		vBox1Min.y < vBox2Max.y && vBox1Max.y > vBox2Min.y && 
		vBox1Min.z < vBox2Max.z && vBox1Max.z > vBox2Min.z;
}


extern float XM_CALLCONV GammaToLinearFullRange( float gamma );
extern float XM_CALLCONV LinearToGammaFullRange( float linear );
extern float XM_CALLCONV GammaToLinear( float gamma );
extern float XM_CALLCONV LinearToGamma( float linear );

extern float XM_CALLCONV SrgbGammaToLinear( float flSrgbGammaValue );
extern float XM_CALLCONV SrgbLinearToGamma( float flLinearValue );
extern float XM_CALLCONV X360GammaToLinear( float fl360GammaValue );
extern float XM_CALLCONV X360LinearToGamma( float flLinearValue );
extern float XM_CALLCONV SrgbGammaTo360Gamma( float flSrgbGammaValue );

// linear (0..4) to screen corrected vertex space (0..1?)
FORCEINLINE float XM_CALLCONV LinearToVertexLight( float f )
{
	extern float lineartovertex[4096];	

	// Gotta clamp before the multiply; could overflow...
	// assume 0..4 range
	int i = RoundFloatToInt( f * 1024.f );

	// Presumably the comman case will be not to clamp, so check that first:
	if( (unsigned)i > 4095 )
	{
		if ( i < 0 )
			i = 0;		// Compare to zero instead of 4095 to save 4 bytes in the instruction stream
		else
			i = 4095;
	}

	return lineartovertex[i];
}


FORCEINLINE unsigned char XM_CALLCONV LinearToLightmap( float f )
{
	extern unsigned char lineartolightmap[4096];	

	// Gotta clamp before the multiply; could overflow...
	int i = RoundFloatToInt( f * 1024.f );	// assume 0..4 range

	// Presumably the comman case will be not to clamp, so check that first:
	if ( (unsigned)i > 4095 )
	{
		if ( i < 0 )
			i = 0;		// Compare to zero instead of 4095 to save 4 bytes in the instruction stream
		else
			i = 4095;
	}

	return lineartolightmap[i];
}

FORCEINLINE void XM_CALLCONV ColorClamp( Vector& color )
{
	float maxc = max( color.x, max( color.y, color.z ) );
	if ( maxc > 1.0f )
	{
		float ooMax = 1.0f / maxc;
		color.x *= ooMax;
		color.y *= ooMax;
		color.z *= ooMax;
	}

	if ( color[0] < 0.f ) color[0] = 0.f;
	if ( color[1] < 0.f ) color[1] = 0.f;
	if ( color[2] < 0.f ) color[2] = 0.f;
}

inline void XM_CALLCONV ColorClampTruncate( Vector& color )
{
	if (color[0] > 1.0f) color[0] = 1.0f; else if (color[0] < 0.0f) color[0] = 0.0f;
	if (color[1] > 1.0f) color[1] = 1.0f; else if (color[1] < 0.0f) color[1] = 0.0f;
	if (color[2] > 1.0f) color[2] = 1.0f; else if (color[2] < 0.0f) color[2] = 0.0f;
}

// Interpolate a Catmull-Rom spline.
// t is a [0,1] value and interpolates a curve between p2 and p3.
void XM_CALLCONV Catmull_Rom_Spline(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector &output );

// Interpolate a Catmull-Rom spline.
// Returns the tangent of the point at t of the spline
void XM_CALLCONV Catmull_Rom_Spline_Tangent( 
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector &output );

// area under the curve [0..t]
void XM_CALLCONV Catmull_Rom_Spline_Integral( 
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector& output );

// area under the curve [0..1]
void XM_CALLCONV Catmull_Rom_Spline_Integral( 
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	Vector& output );

// Interpolate a Catmull-Rom spline.
// Normalize p2->p1 and p3->p4 to be the same length as p2->p3
void XM_CALLCONV Catmull_Rom_Spline_Normalize(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector &output );

// area under the curve [0..t]
// Normalize p2->p1 and p3->p4 to be the same length as p2->p3
void Catmull_Rom_Spline_Integral_Normalize(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector& output );

// Interpolate a Catmull-Rom spline.
// Normalize p2.x->p1.x and p3.x->p4.x to be the same length as p2.x->p3.x
void XM_CALLCONV Catmull_Rom_Spline_NormalizeX(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector &output );

// area under the curve [0..t]
void XM_CALLCONV Catmull_Rom_Spline_NormalizeX(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector& output );

// Interpolate a Hermite spline.
// t is a [0,1] value and interpolates a curve between p1 and p2 with the deltas d1 and d2.
void XM_CALLCONV Hermite_Spline(
	const Vector &p1,
	const Vector &p2,
	const Vector &d1,
	const Vector &d2,
	float t, 
	Vector& output );

float XM_CALLCONV Hermite_Spline(
	float p1,
	float p2,
	float d1,
	float d2,
	float t );

// t is a [0,1] value and interpolates a curve between p1 and p2 with the slopes p0->p1 and p1->p2
void XM_CALLCONV Hermite_Spline(
	const Vector &p0,
	const Vector &p1,
	const Vector &p2,
	float t, 
	Vector& output );

float XM_CALLCONV Hermite_Spline(
	float p0,
	float p1,
	float p2,
	float t );


void XM_CALLCONV Hermite_SplineBasis( float t, float basis[4] );

void XM_CALLCONV Hermite_Spline( 
	const Quaternion &q0, 
	const Quaternion &q1, 
	const Quaternion &q2, 
	float t, 
	Quaternion &output );


// See https://en.wikipedia.org/wiki/Kochanek%E2%80%93Bartels_spline
// 
// Tension:  -1 = Round -> 1 = Tight
// Bias:     -1 = Pre-shoot (bias left) -> 1 = Post-shoot (bias right)
// Continuity: -1 = Box corners -> 1 = Inverted corners
//
// If T=B=C=0 it's the same matrix as Catmull-Rom.
// If T=1 & B=C=0 it's the same as Cubic.
// If T=B=0 & C=-1 it's just linear interpolation
// 
// See http://news.povray.org/povray.binaries.tutorials/attachment/%3CXns91B880592482seed7@povray.org%3E/Splines.bas.txt
// for example code and descriptions of various spline types...
// 
void XM_CALLCONV Kochanek_Bartels_Spline(
	float tension, 
	float bias, 
	float continuity,
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector& output );

void XM_CALLCONV Kochanek_Bartels_Spline_NormalizeX(
	float tension, 
	float bias, 
	float continuity,
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector& output );

// See link at Kochanek_Bartels_Spline for info on the basis matrix used
void XM_CALLCONV Cubic_Spline(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector& output );

void XM_CALLCONV Cubic_Spline_NormalizeX(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector& output );

// See link at Kochanek_Bartels_Spline for info on the basis matrix used
void XM_CALLCONV BSpline(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector& output );

void XM_CALLCONV BSpline_NormalizeX(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector& output );

// See link at Kochanek_Bartels_Spline for info on the basis matrix used
void XM_CALLCONV Parabolic_Spline(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector& output );

void XM_CALLCONV Parabolic_Spline_NormalizeX(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t, 
	Vector& output );

// quintic interpolating polynomial from Perlin.
// 0->0, 1->1, smooth-in between with smooth tangents
FORCEINLINE float QuinticInterpolatingPolynomial(float t)
{
	// 6t^5-15t^4+10t^3
	return t * t * t *( t * ( t* 6.0F - 15.0F ) + 10.0F );
}

// given a table of sorted tabulated positions, return the two indices and blendfactor to linear
// interpolate. Does a search. Can be used to find the blend value to interpolate between
// keyframes.
void XM_CALLCONV GetInterpolationData( float const *pKnotPositions, 
						   float const *pKnotValues,
						   int nNumValuesinList,
						   int nInterpolationRange,
						   float flPositionToInterpolateAt,
						   bool bWrap,
						   float *pValueA, 
						   float *pValueB,
						   float *pInterpolationValue);

float XM_CALLCONV RangeCompressor( float flValue, float flMin, float flMax, float flBase );

// Get the minimum distance from vOrigin to the bounding box defined by [mins,maxs]
// using voronoi regions.
// 0 is returned if the origin is inside the box.
float XM_CALLCONV CalcSqrDistanceToAABB( const Vector &mins, const Vector &maxs, const Vector &point );
void XM_CALLCONV CalcClosestPointOnAABB( Vector mins, Vector maxs, Vector point, Vector &closestOut );
void XM_CALLCONV CalcSqrDistAndClosestPointOnAABB( const Vector &mins, const Vector &maxs, const Vector &point, Vector &closestOut, float &distSqrOut );

inline float XM_CALLCONV CalcDistanceToAABB( const Vector &mins, const Vector &maxs, const Vector &point )
{
	float flDistSqr = CalcSqrDistanceToAABB( mins, maxs, point );
	return sqrtf(flDistSqr);
}

// Get the closest point from P to the (infinite) line through vLineA and vLineB and
// calculate the shortest distance from P to the line.
// If you pass in a value for t, it will tell you the t for (A + (B-A)t) to get the closest point.
// If the closest point lies on the segment between A and B, then 0 <= t <= 1.
void  XM_CALLCONV CalcClosestPointOnLine( const Vector &P, const Vector &vLineA, const Vector &vLineB, Vector &vClosest, float *t=nullptr );
float XM_CALLCONV CalcDistanceToLine( const Vector &P, const Vector &vLineA, const Vector &vLineB, float *t=nullptr );
float XM_CALLCONV CalcDistanceSqrToLine( const Vector &P, const Vector &vLineA, const Vector &vLineB, float *t=nullptr );

// The same three functions as above, except now the line is closed between A and B.
void  XM_CALLCONV CalcClosestPointOnLineSegment( const Vector &P, const Vector &vLineA, const Vector &vLineB, Vector &vClosest, float *t=nullptr );
float XM_CALLCONV CalcDistanceToLineSegment( const Vector &P, const Vector &vLineA, const Vector &vLineB, float *t=nullptr);
float XM_CALLCONV CalcDistanceSqrToLineSegment( const Vector &P, const Vector &vLineA, const Vector &vLineB, float *t=nullptr );

// A function to compute the closes line segment connnection two lines (or false if the lines are parallel, etc.)
bool XM_CALLCONV CalcLineToLineIntersectionSegment(
   const Vector& p1,const Vector& p2,const Vector& p3,const Vector& p4,Vector *s1,Vector *s2,
   float *t1, float *t2 );

// The above functions in 2D
void  XM_CALLCONV CalcClosestPointOnLine2D( Vector2D const &P, Vector2D const &vLineA, Vector2D const &vLineB, Vector2D &vClosest, float *t=nullptr );
float XM_CALLCONV CalcDistanceToLine2D( Vector2D const &P, Vector2D const &vLineA, Vector2D const &vLineB, float *t=nullptr );
float XM_CALLCONV CalcDistanceSqrToLine2D( Vector2D const &P, Vector2D const &vLineA, Vector2D const &vLineB, float *t=nullptr );
void  XM_CALLCONV CalcClosestPointOnLineSegment2D( Vector2D const &P, Vector2D const &vLineA, Vector2D const &vLineB, Vector2D &vClosest, float *t=nullptr );
float XM_CALLCONV CalcDistanceToLineSegment2D( Vector2D const &P, Vector2D const &vLineA, Vector2D const &vLineB, float *t=nullptr );
float XM_CALLCONV CalcDistanceSqrToLineSegment2D( Vector2D const &P, Vector2D const &vLineA, Vector2D const &vLineB, float *t=nullptr );

// Init the mathlib
void XM_CALLCONV MathLib_Init( float gamma = 2.2f, float texGamma = 2.2f, float brightness = 0.0f, int overbright = 2.0f, bool bAllow3DNow = true, bool bAllowSSE = true, bool bAllowSSE2 = true, bool bAllowMMX = true );
bool XM_CALLCONV MathLib_3DNowEnabled( void );
bool XM_CALLCONV MathLib_MMXEnabled( void );
bool XM_CALLCONV MathLib_SSEEnabled( void );
bool XM_CALLCONV MathLib_SSE2Enabled( void );

float XM_CALLCONV Approach( float target, float value, float speed );
float XM_CALLCONV ApproachAngle( float target, float value, float speed );
float XM_CALLCONV AngleDiff( float destAngle, float srcAngle );
float XM_CALLCONV AngleDistance( float next, float cur );
float XM_CALLCONV AngleNormalize( float angle );

// ensure that 0 <= angle <= 360
float XM_CALLCONV AngleNormalizePositive( float angle );

bool XM_CALLCONV AnglesAreEqual( float a, float b, float tolerance = 0.0f );


void XM_CALLCONV RotationDeltaAxisAngle( const QAngle &srcAngles, const QAngle &destAngles, Vector &deltaAxis, float &deltaAngle );
void XM_CALLCONV RotationDelta( const QAngle &srcAngles, const QAngle &destAngles, QAngle *out );

void XM_CALLCONV ComputeTrianglePlane( const Vector& v1, const Vector& v2, const Vector& v3, Vector& normal, float& intercept );
int XM_CALLCONV PolyFromPlane( Vector *outVerts, const Vector& normal, float dist, float fHalfScale = 9000.0f );
int XM_CALLCONV ClipPolyToPlane( Vector *inVerts, int vertCount, Vector *outVerts, const Vector& normal, float dist, float fOnPlaneEpsilon = 0.1f );
int XM_CALLCONV ClipPolyToPlane_Precise( double *inVerts, int vertCount, double *outVerts, const double *normal, double dist, double fOnPlaneEpsilon = 0.1 );

//-----------------------------------------------------------------------------
// Computes a reasonable tangent space for a triangle
//-----------------------------------------------------------------------------
void XM_CALLCONV CalcTriangleTangentSpace( const Vector &p0, const Vector &p1, const Vector &p2,
							  const Vector2D &t0, const Vector2D &t1, const Vector2D& t2,
							  Vector &sVect, Vector &tVect );

//-----------------------------------------------------------------------------
// Transforms a AABB into another space; which will inherently grow the box.
//-----------------------------------------------------------------------------
void XM_CALLCONV TransformAABB( const matrix3x4_t &in1, const Vector &vecMinsIn, const Vector &vecMaxsIn, Vector &vecMinsOut, Vector &vecMaxsOut );

//-----------------------------------------------------------------------------
// Uses the inverse transform of in1
//-----------------------------------------------------------------------------
void XM_CALLCONV ITransformAABB( const matrix3x4_t &in1, const Vector &vecMinsIn, const Vector &vecMaxsIn, Vector &vecMinsOut, Vector &vecMaxsOut );

//-----------------------------------------------------------------------------
// Rotates a AABB into another space; which will inherently grow the box. 
// (same as TransformAABB, but doesn't take the translation into account)
//-----------------------------------------------------------------------------
void XM_CALLCONV RotateAABB( const matrix3x4_t &in1, const Vector &vecMinsIn, const Vector &vecMaxsIn, Vector &vecMinsOut, Vector &vecMaxsOut );

//-----------------------------------------------------------------------------
// Uses the inverse transform of in1
//-----------------------------------------------------------------------------
void XM_CALLCONV IRotateAABB( const matrix3x4_t &in1, Vector vecMinsIn, Vector vecMaxsIn, Vector &vecMinsOut, Vector &vecMaxsOut );

//-----------------------------------------------------------------------------
// Transform a plane
//-----------------------------------------------------------------------------
inline void XM_CALLCONV MatrixTransformPlane( const matrix3x4_t &src, const cplane_t &inPlane, cplane_t &outPlane )
{
	// What we want to do is the following:
	// 1) transform the normal into the new space.
	// 2) Determine a point on the old plane given by plane dist * plane normal
	// 3) Transform that point into the new space
	// 4) Plane dist = DotProduct( new normal, new point )

	// An optimized version, which works if the plane is orthogonal.
	// 1) Transform the normal into the new space
	// 2) Realize that transforming the old plane point into the new space
	// is given by [ d * n'x + Tx, d * n'y + Ty, d * n'z + Tz ]
	// where d = old plane dist, n' = transformed normal, Tn = translational component of transform
	// 3) Compute the new plane dist using the dot product of the normal result of #2

	// For a correct result, this should be an inverse-transpose matrix
	// but that only matters if there are nonuniform scale or skew factors in this matrix.
	VectorRotate( inPlane.normal, src, outPlane.normal );
	outPlane.dist = inPlane.dist * DotProduct( outPlane.normal, outPlane.normal );
	outPlane.dist += outPlane.normal.x * src[0][3] + outPlane.normal.y * src[1][3] + outPlane.normal.z * src[2][3];
}

inline void XM_CALLCONV MatrixITransformPlane( const matrix3x4_t &src, const cplane_t &inPlane, cplane_t &outPlane )
{
	// The trick here is that Tn = translational component of transform,
	// but for an inverse transform, Tn = - R^-1 * T
	Vector vecTranslation;
	MatrixGetColumn( src, 3, vecTranslation );

	Vector vecInvTranslation;
	VectorIRotate( vecTranslation, src, vecInvTranslation );

	VectorIRotate( inPlane.normal, src, outPlane.normal );
	outPlane.dist = inPlane.dist * DotProduct( outPlane.normal, outPlane.normal );
	outPlane.dist -= outPlane.normal.x * vecInvTranslation[0] + outPlane.normal.y * vecInvTranslation[1] + outPlane.normal.z * vecInvTranslation[2];
}

constexpr inline int XM_CALLCONV CeilPow2( int in )
{
	int retval = 1;
	while( retval < in )
		retval <<= 1;
	return retval;
}

constexpr inline int XM_CALLCONV FloorPow2( int in )
{
	int retval = 1;
	while( retval < in )
		retval <<= 1;
	return retval >> 1;
}

FORCEINLINE float * XM_CALLCONV UnpackNormal_HEND3N( const unsigned int *pPackedNormal, float *pNormal )
{
	int temp[3];
	temp[0] = ((*pPackedNormal >> 0L) & 0x7ff);
	if ( temp[0] & 0x400 )
	{
		temp[0] = 2048 - temp[0];
	}
	temp[1] = ((*pPackedNormal >> 11L) & 0x7ff);
	if ( temp[1] & 0x400 )
	{
		temp[1] = 2048 - temp[1];
	}
	temp[2] = ((*pPackedNormal >> 22L) & 0x3ff);
	if ( temp[2] & 0x200 )
	{
		temp[2] = 1024 - temp[2];
	}
	pNormal[0] = (float)temp[0] * 1.0f/1023.0f;
	pNormal[1] = (float)temp[1] * 1.0f/1023.0f;
	pNormal[2] = (float)temp[2] * 1.0f/511.0f;
	return pNormal;
}

FORCEINLINE unsigned int * XM_CALLCONV PackNormal_HEND3N( const float *pNormal, unsigned int *pPackedNormal )
{
	int temp[3];

	temp[0] = Float2Int( pNormal[0] * 1023.0f );
	temp[1] = Float2Int( pNormal[1] * 1023.0f );
	temp[2] = Float2Int( pNormal[2] * 511.0f );

	// the normal is out of bounds, determine the source and fix
	// clamping would be even more of a slowdown here
	Assert( temp[0] >= -1023 && temp[0] <= 1023 );
	Assert( temp[1] >= -1023 && temp[1] <= 1023 );
	Assert( temp[2] >= -511 && temp[2] <= 511 );
	
	*pPackedNormal = ( ( temp[2] & 0x3ff ) << 22L ) |
                     ( ( temp[1] & 0x7ff ) << 11L ) |
                     ( ( temp[0] & 0x7ff ) << 0L );
	return pPackedNormal;
}

FORCEINLINE unsigned int * XM_CALLCONV PackNormal_HEND3N( float nx, float ny, float nz, unsigned int *pPackedNormal )
{
	int temp[3];

	temp[0] = Float2Int( nx * 1023.0f );
	temp[1] = Float2Int( ny * 1023.0f );
	temp[2] = Float2Int( nz * 511.0f );

	// the normal is out of bounds, determine the source and fix
	// clamping would be even more of a slowdown here
	Assert( temp[0] >= -1023 && temp[0] <= 1023 );
	Assert( temp[1] >= -1023 && temp[1] <= 1023 );
	Assert( temp[2] >= -511 && temp[2] <= 511 );
	
	*pPackedNormal = ( ( temp[2] & 0x3ff ) << 22L ) |
                     ( ( temp[1] & 0x7ff ) << 11L ) |
                     ( ( temp[0] & 0x7ff ) << 0L );
	return pPackedNormal;
}

FORCEINLINE float * XM_CALLCONV UnpackNormal_SHORT2( const unsigned int *pPackedNormal, float *pNormal, bool bIsTangent = FALSE )
{
	// Unpacks from Jason's 2-short format (fills in a 4th binormal-sign (+1/-1) value, if this is a tangent vector)

	// FIXME: short math is slow on 360 - use ints here instead (bit-twiddle to deal w/ the sign bits)
	short iX = (*pPackedNormal & 0x0000FFFF);
	short iY = (*pPackedNormal & 0xFFFF0000) >> 16;

	float zSign = +1;
	if ( iX < 0 )
	{
		zSign = -1;
		iX    = -iX;
	}
	float tSign = +1;
	if ( iY < 0 )
	{
		tSign = -1;
		iY    = -iY;
	}

	pNormal[0] = ( iX - 16384.0f ) / 16384.0f;
	pNormal[1] = ( iY - 16384.0f ) / 16384.0f;
	pNormal[2] = zSign*sqrtf( 1.0f - ( pNormal[0]*pNormal[0] + pNormal[1]*pNormal[1] ) );
	if ( bIsTangent )
	{
		pNormal[3] = tSign;
	}

	return pNormal;
}

FORCEINLINE unsigned int * XM_CALLCONV PackNormal_SHORT2( float nx, float ny, float nz, unsigned int *pPackedNormal, float binormalSign = +1.0f )
{
	// Pack a vector (ASSUMED TO BE NORMALIZED) into Jason's 4-byte (SHORT2) format.
	// This simply reconstructs Z from X & Y. It uses the sign bits of the X & Y coords
	// to reconstruct the sign of Z and, if this is a tangent vector, the sign of the
	// binormal (this is needed because tangent/binormal vectors are supposed to follow
	// UV gradients, but shaders reconstruct the binormal from the tangent and normal
	// assuming that they form a right-handed basis).

	nx += 1;					// [-1,+1] -> [0,2]
	ny += 1;
	nx *= 16384.0f;				// [ 0, 2] -> [0,32768]
	ny *= 16384.0f;

	// '0' and '32768' values are invalid encodings
	nx = max( nx, 1.0f );		// Make sure there are no zero values
	ny = max( ny, 1.0f );
	nx = min( nx, 32767.0f );	// Make sure there are no 32768 values
	ny = min( ny, 32767.0f );

	if ( nz < 0.0f )
		nx = -nx;				// Set the sign bit for z

	ny *= binormalSign;			// Set the sign bit for the binormal (use when encoding a tangent vector)

	// FIXME: short math is slow on 360 - use ints here instead (bit-twiddle to deal w/ the sign bits), also use Float2Int()
	short sX = (short)nx;		// signed short [1,32767]
	short sY = (short)ny;

	*pPackedNormal = ( sX & 0x0000FFFF ) | ( sY << 16 ); // NOTE: The mask is necessary (if sX is negative and cast to an int...)

	return pPackedNormal;
}

FORCEINLINE unsigned int * XM_CALLCONV PackNormal_SHORT2( const float *pNormal, unsigned int *pPackedNormal, float binormalSign = +1.0f )
{
	return PackNormal_SHORT2( pNormal[0], pNormal[1], pNormal[2], pPackedNormal, binormalSign );
}

// Unpacks a UBYTE4 normal (for a tangent, the result's fourth component receives the binormal 'sign')
FORCEINLINE float * XM_CALLCONV UnpackNormal_UBYTE4( const unsigned int *pPackedNormal, float *pNormal, bool bIsTangent = FALSE )
{
	unsigned char cX, cY;
	if ( bIsTangent )
	{
		cX = static_cast<unsigned char>(*pPackedNormal >> 16);					// Unpack Z
		cY = static_cast<unsigned char>(*pPackedNormal >> 24);					// Unpack W
	}
	else
	{
		cX = static_cast<unsigned char>(*pPackedNormal >>  0);					// Unpack X
		cY = static_cast<unsigned char>(*pPackedNormal >>  8);					// Unpack Y
	}

	float x = cX - 128.0f;
	float y = cY - 128.0f;
	float z;

	float zSignBit = x < 0 ? 1.0f : 0.0f;			// z and t negative bits (like slt asm instruction)
	float tSignBit = y < 0 ? 1.0f : 0.0f;
	float zSign    = -( 2*zSignBit - 1 );			// z and t signs
	float tSign    = -( 2*tSignBit - 1 );

	x = x*zSign - zSignBit;							// 0..127
	y = y*tSign - tSignBit;
	x = x - 64;										// -64..63
	y = y - 64;

	float xSignBit = x < 0 ? 1.0f : 0.0f;	// x and y negative bits (like slt asm instruction)
	float ySignBit = y < 0 ? 1.0f : 0.0f;
	float xSign    = -( 2*xSignBit - 1 );			// x and y signs
	float ySign    = -( 2*ySignBit - 1 );

	x = ( x*xSign - xSignBit ) / 63.0f;				// 0..1 range
	y = ( y*ySign - ySignBit ) / 63.0f;
	z = 1.0f - x - y;

	float oolen	 = 1.0f / sqrtf( x*x + y*y + z*z );	// Normalize and
	x			*= oolen * xSign;					// Recover signs
	y			*= oolen * ySign;
	z			*= oolen * zSign;

	pNormal[0] = x;
	pNormal[1] = y;
	pNormal[2] = z;
	if ( bIsTangent )
	{
		pNormal[3] = tSign;
	}

	return pNormal;
}

//////////////////////////////////////////////////////////////////////////////
// See: http://www.oroboro.com/rafael/docserv.php/index/programming/article/unitv2
//
// UBYTE4 encoding, using per-octant projection onto x+y+z=1
// Assume input vector is already unit length
//
// binormalSign specifies 'sign' of binormal, stored in t sign bit of tangent
// (lets the shader know whether norm/tan/bin form a right-handed basis)
//
// bIsTangent is used to specify which WORD of the output to store the data
// The expected usage is to call once with the normal and once with
// the tangent and binormal sign flag, bitwise OR'ing the returned DWORDs
FORCEINLINE unsigned int * XM_CALLCONV PackNormal_UBYTE4( float nx, float ny, float nz, unsigned int *pPackedNormal, bool bIsTangent = false, float binormalSign = +1.0f )
{
	float xSign = nx < 0.0f ? -1.0f : 1.0f;			// -1 or 1 sign
	float ySign = ny < 0.0f ? -1.0f : 1.0f;
	float zSign = nz < 0.0f ? -1.0f : 1.0f;
	float tSign = binormalSign;
	Assert( ( binormalSign == +1.0f ) || ( binormalSign == -1.0f ) );

	float xSignBit = 0.5f*( 1 - xSign );			// [-1,+1] -> [1,0]
	float ySignBit = 0.5f*( 1 - ySign );			// 1 is negative bit (like slt instruction)
	float zSignBit = 0.5f*( 1 - zSign );
	float tSignBit = 0.5f*( 1 - binormalSign );		

	float absX = xSign*nx;							// 0..1 range (abs)
	float absY = ySign*ny;
	float absZ = zSign*nz;

	float xbits = absX / ( absX + absY + absZ );	// Project onto x+y+z=1 plane
	float ybits = absY / ( absX + absY + absZ );

	xbits *= 63;									// 0..63
	ybits *= 63;

	xbits  = xbits * xSign - xSignBit;				// -64..63 range
	ybits  = ybits * ySign - ySignBit;
	xbits += 64.0f;									// 0..127 range
	ybits += 64.0f;

	xbits  = xbits * zSign - zSignBit;				// Negate based on z and t
	ybits  = ybits * tSign - tSignBit;				// -128..127 range

	xbits += 128.0f;								// 0..255 range
	ybits += 128.0f;

	unsigned char cX = (unsigned char) xbits;
	unsigned char cY = (unsigned char) ybits;

	if ( !bIsTangent )
		*pPackedNormal = (cX <<  0) | (cY <<  8);	// xy for normal
	else						   
		*pPackedNormal = (cX << 16) | (cY << 24);	// zw for tangent

	return pPackedNormal;
}

FORCEINLINE unsigned int * XM_CALLCONV PackNormal_UBYTE4( const float *pNormal, unsigned int *pPackedNormal, bool bIsTangent = false, float binormalSign = +1.0f )
{
	return PackNormal_UBYTE4( pNormal[0], pNormal[1], pNormal[2], pPackedNormal, bIsTangent, binormalSign );
}


//-----------------------------------------------------------------------------
// Convert RGB to HSV
//-----------------------------------------------------------------------------
void RGBtoHSV( const Vector &rgb, Vector &hsv );


//-----------------------------------------------------------------------------
// Convert HSV to RGB
//-----------------------------------------------------------------------------
void XM_CALLCONV HSVtoRGB( const Vector &hsv, Vector &rgb );


//-----------------------------------------------------------------------------
// Fast version of pow and log
//-----------------------------------------------------------------------------

float XM_CALLCONV FastLog2(float i);			// log2( i )
float XM_CALLCONV FastPow2(float i);			// 2^i
float XM_CALLCONV FastPow(float a, float b);	// a^b
float XM_CALLCONV FastPow10( float i );			// 10^i

//-----------------------------------------------------------------------------
// For testing float equality
//-----------------------------------------------------------------------------

inline bool XM_CALLCONV CloseEnough( float a, float b, float epsilon = EQUAL_EPSILON )
{
	return fabsf( a - b ) <= epsilon;
}

inline bool XM_CALLCONV CloseEnough( const Vector &a, const Vector &b, float epsilon = EQUAL_EPSILON )
{
	return
		CloseEnough( a.x, b.x, epsilon ) &&
		CloseEnough( a.y, b.y, epsilon ) &&
		CloseEnough( a.z, b.z, epsilon );
}

// Fast compare.
// 
// maxUlps is the maximum error in terms of Units in the Last Place.  This
// specifies how big an error we are willing to accept in terms of the value
// of the least significant digit of the floating point number s representation.
// maxUlps can also be interpreted in terms of how many representable floats we
// are willing to accept between A and B.  This function will allow maxUlps-1
// floats between A and B.
bool XM_CALLCONV AlmostEqual(float a, float b, int maxUlps = 10);

inline bool XM_CALLCONV AlmostEqual( const Vector &a, const Vector &b, int maxUlps = 10)
{
	return AlmostEqual( a.x, b.x, maxUlps ) &&
		AlmostEqual( a.y, b.y, maxUlps ) &&
		AlmostEqual( a.z, b.z, maxUlps );
}


#endif	// MATH_BASE_H

