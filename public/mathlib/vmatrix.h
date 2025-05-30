//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// VMatrix always postmultiply vectors as in Ax = b.
// Given a set of basis vectors ((F)orward, (L)eft, (U)p), and a (T)ranslation, 
// a matrix to transform a vector into that space looks like this:
// Fx Lx Ux Tx
// Fy Ly Uy Ty
// Fz Lz Uz Tz
// 0   0  0  1

// Note that concatenating matrices needs to multiply them in reverse order.
// ie: if I want to apply matrix A, B, then C, the equation needs to look like this:
// C * B * A * v
// ie:
// v = A * v;
// v = B * v;
// v = C * v;
//=============================================================================

#ifndef VMATRIX_H
#define VMATRIX_H

#ifdef _WIN32
#pragma once
#endif

#include <DirectXMath.h>
#include <cstring>

#include "mathlib/vector.h"
#include "mathlib/vplane.h"
#include "mathlib/vector4d.h"
#include "mathlib/mathlib.h"

struct cplane_t;


class VMatrix
{
public:

	VMatrix() = default;
	VMatrix(
		vec_t m00, vec_t m01, vec_t m02, vec_t m03,
		vec_t m10, vec_t m11, vec_t m12, vec_t m13,
		vec_t m20, vec_t m21, vec_t m22, vec_t m23,
		vec_t m30, vec_t m31, vec_t m32, vec_t m33
		);

	// Creates a matrix where the X axis = forward
	// the Y axis = left, and the Z axis = up
	VMatrix( const Vector& forward, const Vector& left, const Vector& up );
	VMatrix( const Vector& forward, const Vector& left, const Vector& up, const Vector& translation );
	
	// Construct from a 3x4 matrix
	VMatrix( const matrix3x4_t& matrix3x4 );

	// Set the values in the matrix.
	void		Init( 
		vec_t m00, vec_t m01, vec_t m02, vec_t m03,
		vec_t m10, vec_t m11, vec_t m12, vec_t m13,
		vec_t m20, vec_t m21, vec_t m22, vec_t m23,
		vec_t m30, vec_t m31, vec_t m32, vec_t m33 
		);


	// Initialize from a 3x4
	void		Init( const matrix3x4_t& matrix3x4 );

	// array access
	[[nodiscard]] inline float* operator[](int i) //-V302
	{ 
		return m[i]; 
	}

	[[nodiscard]] inline const float* operator[](int i) const //-V302
	{ 
		return m[i]; 
	}

	// Get a pointer to m[0][0]
	[[nodiscard]] inline float *Base()
	{
		return &m[0][0];
	}

	[[nodiscard]] inline const float *Base() const
	{
		return &m[0][0];
	}

	// dimhotepus: Better DirectX math integration.
	[[nodiscard]] DirectX::XMFLOAT4* XmBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT4) == sizeof(m[0]));
		static_assert(alignof(DirectX::XMFLOAT4) == alignof(decltype(m)));
		return reinterpret_cast<DirectX::XMFLOAT4*>(&m[0][0]);
	}
	[[nodiscard]] DirectX::XMFLOAT4 const* XmBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT4) == sizeof(m[0]));
		static_assert(alignof(DirectX::XMFLOAT4) == alignof(decltype(m)));
		return reinterpret_cast<DirectX::XMFLOAT4 const*>(&m[0][0]);
	}

	// dimhotepus: Better DirectX math integration.
	[[nodiscard]] DirectX::XMFLOAT4X4* XmMBase()
	{
		static_assert(sizeof(DirectX::XMFLOAT4X4) == sizeof(m));
		static_assert(alignof(DirectX::XMFLOAT4X4) == alignof(decltype(m)));
		return reinterpret_cast<DirectX::XMFLOAT4X4*>(&m[0][0]);
	}
	[[nodiscard]] DirectX::XMFLOAT4X4 const* XmMBase() const
	{
		static_assert(sizeof(DirectX::XMFLOAT4X4) == sizeof(m));
		static_assert(alignof(DirectX::XMFLOAT4X4) == alignof(decltype(m)));
		return reinterpret_cast<DirectX::XMFLOAT4X4 const*>(&m[0][0]);
	}

	void		SetLeft(const Vector &vLeft);
	void		SetUp(const Vector &vUp);
	void		SetForward(const Vector &vForward);

	void		GetBasisVectors(Vector &vForward, Vector &vLeft, Vector &vUp) const;
	void		SetBasisVectors(const Vector &vForward, const Vector &vLeft, const Vector &vUp);

	// Get/set the translation.
	Vector &	GetTranslation( Vector &vTrans ) const;
	void		SetTranslation(const Vector &vTrans);

	void		PreTranslate(const Vector &vTrans);
	void		PostTranslate(const Vector &vTrans);

	[[nodiscard]] const matrix3x4_t& As3x4() const;
	// dimhotepus: Add non-const version as it is used.
	[[nodiscard]] matrix3x4_t& As3x4();
	void		CopyFrom3x4( const matrix3x4_t &m3x4 );
	void		Set3x4( matrix3x4_t& matrix3x4 ) const;

	[[nodiscard]] bool		operator==( const VMatrix& src ) const;
	[[nodiscard]] bool		operator!=( const VMatrix& src ) const { return !( *this == src ); }

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// Access the basis vectors.
	[[nodiscard]] Vector		GetLeft() const;
	[[nodiscard]] Vector		GetUp() const;
	[[nodiscard]] Vector		GetForward() const;
	[[nodiscard]] Vector		GetTranslation() const;
#endif


// Matrix->vector operations.
public:
	// Multiply by a 3D vector (same as operator*).
	void		V3Mul(const Vector &vIn, Vector &vOut) const;

	// Multiply by a 4D vector.
	void		V4Mul(const Vector4D &vIn, Vector4D &vOut) const;

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// Applies the rotation (ignores translation in the matrix). (This just calls VMul3x3).
	[[nodiscard]] Vector		ApplyRotation(const Vector &vVec) const;

	// Multiply by a vector (divides by w, assumes input w is 1).
	[[nodiscard]] Vector		operator*(const Vector &vVec) const;

	// Multiply by the upper 3x3 part of the matrix (ie: only apply rotation).
	[[nodiscard]] Vector		VMul3x3(const Vector &vVec) const;

	// Apply the inverse (transposed) rotation (only works on pure rotation matrix)
	[[nodiscard]] Vector		VMul3x3Transpose(const Vector &vVec) const;

	// Multiply by the upper 3 rows.
	[[nodiscard]] Vector		VMul4x3(const Vector &vVec) const;

	// Apply the inverse (transposed) transformation (only works on pure rotation/translation)
	[[nodiscard]] Vector		VMul4x3Transpose(const Vector &vVec) const;
#endif


// Matrix->plane operations.
public:
	// Transform the plane. The matrix can only contain translation and rotation.
	void		TransformPlane( const VPlane &inPlane, VPlane &outPlane ) const;

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// Just calls TransformPlane and returns the result.
	[[nodiscard]] VPlane		operator*(const VPlane &thePlane) const;
#endif

// Matrix->matrix operations.
public:
	// Multiply two matrices (out = this * vm).
	void		MatrixMul( const VMatrix &vm, VMatrix &out ) const;

	// Add two matrices.
	const VMatrix& operator+=(const VMatrix &other);

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// Just calls MatrixMul and returns the result.	
	[[nodiscard]] VMatrix		operator*(const VMatrix &mOther) const;

	// Add/Subtract two matrices.
	[[nodiscard]] VMatrix		operator+(const VMatrix &other) const;
	[[nodiscard]] VMatrix		operator-(const VMatrix &other) const;

	// Negation.
	[[nodiscard]] VMatrix		operator-() const;

	// Return inverse matrix. Be careful because the results are undefined 
	// if the matrix doesn't have an inverse (ie: InverseGeneral returns false).
	[[nodiscard]] VMatrix		operator~() const;
#endif

// Matrix operations.
public:
	// Set to identity.
	void		Identity();

	[[nodiscard]] bool		IsIdentity() const;

	// Setup a matrix for origin and angles.
	void		SetupMatrixOrgAngles( const Vector &origin, const QAngle &vAngles );
	
	// Setup a matrix for angles and no translation.
	void		SetupMatrixAngles( const QAngle &vAngles );

	// General inverse. This may fail so check the return!
	[[nodiscard]] bool		InverseGeneral(VMatrix &vInverse) const;
	
	// Does a fast inverse, assuming the matrix only contains translation and rotation.
	void		InverseTR( VMatrix &mRet ) const;

	// Usually used for debug checks. Returns true if the upper 3x3 contains
	// unit vectors and they are all orthogonal.
	[[nodiscard]] bool		IsRotationMatrix() const;
	
#ifndef VECTOR_NO_SLOW_OPERATIONS
	// This calls the other InverseTR and returns the result.
	[[nodiscard]] VMatrix		InverseTR() const;

	// Get the scale of the matrix's basis vectors.
	[[nodiscard]] Vector		GetScale() const;

	// (Fast) multiply by a scaling matrix setup from vScale.
	[[nodiscard]] VMatrix		Scale(const Vector &vScale);	

	// Normalize the basis vectors.
	[[nodiscard]] VMatrix		NormalizeBasisVectors() const;

	// Transpose.
	[[nodiscard]] VMatrix		Transpose() const;

	// Transpose upper-left 3x3.
	[[nodiscard]] VMatrix		Transpose3x3() const;
#endif

public:
	// The matrix.
	vec_t		m[4][4];
};



//-----------------------------------------------------------------------------
// Helper functions.
//-----------------------------------------------------------------------------

#ifndef VECTOR_NO_SLOW_OPERATIONS

// Setup an identity matrix.
[[nodiscard]] VMatrix		SetupMatrixIdentity();

// Setup as a scaling matrix.
[[nodiscard]] VMatrix		SetupMatrixScale(const Vector &vScale);

// Setup a translation matrix.
[[nodiscard]] VMatrix		SetupMatrixTranslation(const Vector &vTranslation);

// Setup a matrix to reflect around the plane.
[[nodiscard]] VMatrix		SetupMatrixReflection(const VPlane &thePlane);

// Setup a matrix to project from vOrigin onto thePlane.
[[nodiscard]] VMatrix		SetupMatrixProjection(const Vector &vOrigin, const VPlane &thePlane);

// Setup a matrix to rotate the specified amount around the specified axis.
[[nodiscard]] VMatrix		SetupMatrixAxisRot(const Vector &vAxis, vec_t fDegrees);

// Setup a matrix from euler angles. Just sets identity and calls MatrixAngles.
[[nodiscard]] VMatrix		SetupMatrixAngles(const QAngle &vAngles);

// Setup a matrix for origin and angles.
[[nodiscard]] VMatrix		SetupMatrixOrgAngles(const Vector &origin, const QAngle &vAngles);

#endif

#define VMatToString(mat)	(static_cast<const char *>(CFmtStr("[ (%f, %f, %f), (%f, %f, %f), (%f, %f, %f), (%f, %f, %f) ]", mat.m[0][0], mat.m[0][1], mat.m[0][2], mat.m[0][3], mat.m[1][0], mat.m[1][1], mat.m[1][2], mat.m[1][3], mat.m[2][0], mat.m[2][1], mat.m[2][2], mat.m[2][3], mat.m[3][0], mat.m[3][1], mat.m[3][2], mat.m[3][3] ))) // ** Note: this generates a temporary, don't hold reference!

//-----------------------------------------------------------------------------
// Returns the point at the intersection on the 3 planes.
// Returns false if it can't be solved (2 or more planes are parallel).
//-----------------------------------------------------------------------------
[[nodiscard]] bool PlaneIntersection( const VPlane &vp1, const VPlane &vp2, const VPlane &vp3, Vector &vOut );


//-----------------------------------------------------------------------------
// These methods are faster. Use them if you want faster code
//-----------------------------------------------------------------------------
void MatrixSetIdentity( VMatrix &dst );
void MatrixTranspose( const VMatrix& src, VMatrix& dst );
void MatrixCopy( const VMatrix& src, VMatrix& dst );
void MatrixMultiply( const VMatrix& src1, const VMatrix& src2, VMatrix& dst );

// Accessors
void MatrixGetColumn( const VMatrix &src, int nCol, Vector *pColumn );
void MatrixSetColumn( VMatrix &src, int nCol, const Vector &column );
void MatrixGetRow( const VMatrix &src, int nCol, Vector *pColumn );
void MatrixSetRow( VMatrix &src, int nCol, const Vector &column );

// Vector3DMultiply treats src2 as if it's a direction vector
void Vector3DMultiply( const VMatrix& src1, const Vector& src2, Vector& dst );

// Vector3DMultiplyPosition treats src2 as if it's a point (adds the translation)
inline void Vector3DMultiplyPosition( const VMatrix& src1, const VectorByValue src2, Vector& dst );

// Vector3DMultiplyPositionProjective treats src2 as if it's a point 
// and does the perspective divide at the end
void Vector3DMultiplyPositionProjective( const VMatrix& src1, const Vector &src2, Vector& dst );

// Vector3DMultiplyPosition treats src2 as if it's a direction 
// and does the perspective divide at the end
// NOTE: src1 had better be an inverse transpose to use this correctly
void Vector3DMultiplyProjective( const VMatrix& src1, const Vector &src2, Vector& dst );

void Vector4DMultiply( const VMatrix& src1, const Vector4D& src2, Vector4D& dst );

// Same as Vector4DMultiply except that src2 has an implicit W of 1
void Vector4DMultiplyPosition( const VMatrix& src1, const Vector &src2, Vector4D& dst );

// Multiplies the vector by the transpose of the matrix
void Vector3DMultiplyTranspose( const VMatrix& src1, const Vector& src2, Vector& dst );
void Vector4DMultiplyTranspose( const VMatrix& src1, const Vector4D& src2, Vector4D& dst );

// Transform a plane
void MatrixTransformPlane( const VMatrix &src, const cplane_t &inPlane, cplane_t &outPlane );

// Transform a plane that has an axis-aligned normal
void MatrixTransformAxisAlignedPlane( const VMatrix &src, int nDim, float flSign, float flDist, cplane_t &outPlane );

void MatrixBuildTranslation( VMatrix& dst, float x, float y, float z );
void MatrixBuildTranslation( VMatrix& dst, const Vector &translation );

inline void MatrixTranslate( VMatrix& dst, const Vector &translation )
{
	VMatrix matTranslation, temp;
	MatrixBuildTranslation( matTranslation, translation );
	MatrixMultiply( dst, matTranslation, temp );
	dst = temp;
}

[[nodiscard]] inline DirectX::XMMATRIX MatrixBuildTranslation(float x, float y, float z) {
	DirectX::XMMATRIX dst = DirectX::XMMatrixIdentity();

	dst.r[0] = DirectX::XMVectorSetW( dst.r[0], x ); //-V2002
	dst.r[1] = DirectX::XMVectorSetW( dst.r[1], y ); //-V2002
	dst.r[2] = DirectX::XMVectorSetW( dst.r[2], z ); //-V2002

	return dst;
}


void MatrixBuildRotationAboutAxis( VMatrix& dst, const Vector& vAxisOfRot, float angleDegrees );
void MatrixBuildRotateZ( VMatrix& dst, float angleDegrees );

[[nodiscard]] inline DirectX::XMMATRIX MatrixBuildRotateZ( float angleDegrees )
{
	float fSin, fCos;
	DirectX::XMScalarSinCos( &fSin, &fCos, DEG2RAD( angleDegrees ) );

	DirectX::XMMATRIX dst = DirectX::XMMatrixIdentity();
	DirectX::XMVECTOR sinCos = DirectX::XMVectorSet(fCos, -fSin, fSin, fCos);

	dst.r[0] = DirectX::XMVectorPermute
	<
		DirectX::XM_PERMUTE_1X,
		DirectX::XM_PERMUTE_1Y,
		DirectX::XM_PERMUTE_0Z,
		DirectX::XM_PERMUTE_0W
	>(dst.r[0], sinCos);
	dst.r[1] = DirectX::XMVectorPermute
	<
		DirectX::XM_PERMUTE_1Z,
		DirectX::XM_PERMUTE_1W,
		DirectX::XM_PERMUTE_0Z,
		DirectX::XM_PERMUTE_0W
	>(dst.r[1], sinCos);

	return dst;
}

inline void MatrixRotate( VMatrix& dst, const Vector& vAxisOfRot, float angleDegrees )
{
	VMatrix rotation, temp;
	MatrixBuildRotationAboutAxis( rotation, vAxisOfRot, angleDegrees );
	MatrixMultiply( dst, rotation, temp );
	dst = temp;
}

// Builds a rotation matrix that rotates one direction vector into another
void MatrixBuildRotation( VMatrix &dst, const Vector& initialDirection, const Vector& finalDirection );

// Builds a scale matrix
void MatrixBuildScale( VMatrix &dst, float x, float y, float z );
void MatrixBuildScale( VMatrix &dst, const Vector& scale );

// Build a perspective matrix.
// zNear and zFar are assumed to be positive.
// You end up looking down positive Z, X is to the right, Y is up.
// X range: [0..1]
// Y range: [0..1]
// Z range: [0..1]
void MatrixBuildPerspective( VMatrix &dst, float fovX, float fovY, float zNear, float zFar );

//-----------------------------------------------------------------------------
// Given a projection matrix, take the extremes of the space in transformed into world space and
// get a bounding box.
//-----------------------------------------------------------------------------
void CalculateAABBFromProjectionMatrix( const VMatrix &worldToVolume, Vector *pMins, Vector *pMaxs );

//-----------------------------------------------------------------------------
// Given a projection matrix, take the extremes of the space in transformed into world space and
// get a bounding sphere.
//-----------------------------------------------------------------------------
void CalculateSphereFromProjectionMatrix( const VMatrix &worldToVolume, Vector *pCenter, float *pflRadius );

//-----------------------------------------------------------------------------
// Given an inverse projection matrix, take the extremes of the space in transformed into world space and
// get a bounding box.
//-----------------------------------------------------------------------------
void CalculateAABBFromProjectionMatrixInverse( const VMatrix &volumeToWorld, Vector *pMins, Vector *pMaxs );

//-----------------------------------------------------------------------------
// Given an inverse projection matrix, take the extremes of the space in transformed into world space and
// get a bounding sphere.
//-----------------------------------------------------------------------------
void CalculateSphereFromProjectionMatrixInverse( const VMatrix &volumeToWorld, Vector *pCenter, float *pflRadius );

//-----------------------------------------------------------------------------
// Calculate frustum planes given a clip->world space transform.
//-----------------------------------------------------------------------------
void FrustumPlanesFromMatrix( const VMatrix &clipToWorld, Frustum_t &frustum );

//-----------------------------------------------------------------------------
// Setup a matrix from euler angles. 
//-----------------------------------------------------------------------------
void MatrixFromAngles( const QAngle& vAngles, VMatrix& dst );

//-----------------------------------------------------------------------------
// Creates euler angles from a matrix 
//-----------------------------------------------------------------------------
void MatrixToAngles( const VMatrix& src, QAngle& vAngles );

//-----------------------------------------------------------------------------
// Does a fast inverse, assuming the matrix only contains translation and rotation.
//-----------------------------------------------------------------------------
void MatrixInverseTR( const VMatrix& src, VMatrix &dst );

//-----------------------------------------------------------------------------
// Inverts any matrix at all
//-----------------------------------------------------------------------------
bool MatrixInverseGeneral(const VMatrix& src, VMatrix& dst);

//-----------------------------------------------------------------------------
// Computes the inverse transpose
//-----------------------------------------------------------------------------
void MatrixInverseTranspose( const VMatrix& src, VMatrix& dst );



//-----------------------------------------------------------------------------
// VMatrix inlines.
//-----------------------------------------------------------------------------
inline VMatrix::VMatrix(
	vec_t m00, vec_t m01, vec_t m02, vec_t m03,
	vec_t m10, vec_t m11, vec_t m12, vec_t m13,
	vec_t m20, vec_t m21, vec_t m22, vec_t m23,
	vec_t m30, vec_t m31, vec_t m32, vec_t m33)
{
	Init(
		m00, m01, m02, m03,
		m10, m11, m12, m13,
		m20, m21, m22, m23,
		m30, m31, m32, m33
		);
}


inline VMatrix::VMatrix( const matrix3x4_t& matrix3x4 )
{
	Init( matrix3x4 );
}


//-----------------------------------------------------------------------------
// Creates a matrix where the X axis = forward
// the Y axis = left, and the Z axis = up
//-----------------------------------------------------------------------------
inline VMatrix::VMatrix( const Vector& xAxis, const Vector& yAxis, const Vector& zAxis )
{
	Init(
		xAxis.x, yAxis.x, zAxis.x, 0.0f,
		xAxis.y, yAxis.y, zAxis.y, 0.0f,
		xAxis.z, yAxis.z, zAxis.z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);
}

inline VMatrix::VMatrix( const Vector& xAxis, const Vector& yAxis, const Vector& zAxis, const Vector& translation )
{
	Init(
		xAxis.x, yAxis.x, zAxis.x, translation.x,
		xAxis.y, yAxis.y, zAxis.y, translation.y,
		xAxis.z, yAxis.z, zAxis.z, translation.z,
		0.0f, 0.0f, 0.0f, 1.0f
		);
}


inline void VMatrix::Init(
	vec_t m00, vec_t m01, vec_t m02, vec_t m03,
	vec_t m10, vec_t m11, vec_t m12, vec_t m13,
	vec_t m20, vec_t m21, vec_t m22, vec_t m23,
	vec_t m30, vec_t m31, vec_t m32, vec_t m33
	)
{
	m[0][0] = m00;
	m[0][1] = m01;
	m[0][2] = m02;
	m[0][3] = m03;

	m[1][0] = m10;
	m[1][1] = m11;
	m[1][2] = m12;
	m[1][3] = m13;

	m[2][0] = m20;
	m[2][1] = m21;
	m[2][2] = m22;
	m[2][3] = m23;

	m[3][0] = m30;
	m[3][1] = m31;
	m[3][2] = m32;
	m[3][3] = m33;
}


//-----------------------------------------------------------------------------
// Initialize from a 3x4
//-----------------------------------------------------------------------------
inline void VMatrix::Init( const matrix3x4_t& matrix3x4 )
{
	memcpy(m, matrix3x4.Base(), sizeof( matrix3x4_t ) );

	m[3][0] = 0.0f;
	m[3][1] = 0.0f;
	m[3][2] = 0.0f;
	m[3][3] = 1.0f;
}


//-----------------------------------------------------------------------------
// Methods related to the basis vectors of the matrix
//-----------------------------------------------------------------------------

#ifndef VECTOR_NO_SLOW_OPERATIONS

inline Vector VMatrix::GetForward() const
{
	return Vector(m[0][0], m[1][0], m[2][0]);
}

inline Vector VMatrix::GetLeft() const
{
	return Vector(m[0][1], m[1][1], m[2][1]);
}

inline Vector VMatrix::GetUp() const
{
	return Vector(m[0][2], m[1][2], m[2][2]);
}

#endif

inline void VMatrix::SetForward(const Vector &vForward)
{
	m[0][0] = vForward.x;
	m[1][0] = vForward.y;
	m[2][0] = vForward.z;
}

inline void VMatrix::SetLeft(const Vector &vLeft)
{
	m[0][1] = vLeft.x;
	m[1][1] = vLeft.y;
	m[2][1] = vLeft.z;
}

inline void VMatrix::SetUp(const Vector &vUp)
{
	m[0][2] = vUp.x;
	m[1][2] = vUp.y;
	m[2][2] = vUp.z;
}

inline void VMatrix::GetBasisVectors(Vector &vForward, Vector &vLeft, Vector &vUp) const
{
	vForward.Init( m[0][0], m[1][0], m[2][0] );
	vLeft.Init( m[0][1], m[1][1], m[2][1] );
	vUp.Init( m[0][2], m[1][2], m[2][2] );
}

inline void VMatrix::SetBasisVectors(const Vector &vForward, const Vector &vLeft, const Vector &vUp)
{
	SetForward(vForward);
	SetLeft(vLeft);
	SetUp(vUp);
}


//-----------------------------------------------------------------------------
// Methods related to the translation component of the matrix
//-----------------------------------------------------------------------------
#ifndef VECTOR_NO_SLOW_OPERATIONS

inline Vector VMatrix::GetTranslation() const
{
	return Vector(m[0][3], m[1][3], m[2][3]);
}

#endif

inline Vector& VMatrix::GetTranslation( Vector &vTrans ) const
{
	vTrans.x = m[0][3];
	vTrans.y = m[1][3];
	vTrans.z = m[2][3];
	return vTrans;
}

inline void VMatrix::SetTranslation(const Vector &vTrans)
{
	m[0][3] = vTrans.x;
	m[1][3] = vTrans.y;
	m[2][3] = vTrans.z;
}

		  
//-----------------------------------------------------------------------------
// appply translation to this matrix in the input space
//-----------------------------------------------------------------------------
inline void VMatrix::PreTranslate(const Vector &vTrans)
{
	Vector tmp;
	Vector3DMultiplyPosition( *this, vTrans, tmp );
	m[0][3] = tmp.x;
	m[1][3] = tmp.y;
	m[2][3] = tmp.z;
}


//-----------------------------------------------------------------------------
// appply translation to this matrix in the output space
//-----------------------------------------------------------------------------
inline void VMatrix::PostTranslate(const Vector &vTrans)
{
	m[0][3] += vTrans.x;
	m[1][3] += vTrans.y;
	m[2][3] += vTrans.z;
}

inline const matrix3x4_t& VMatrix::As3x4() const
{
	static_assert(alignof(VMatrix) == alignof(matrix3x4_t));
	static_assert(sizeof(VMatrix) >= sizeof(matrix3x4_t));
	return *reinterpret_cast<const matrix3x4_t*>(this);
}

inline matrix3x4_t& VMatrix::As3x4()
{
	static_assert(alignof(VMatrix) == alignof(matrix3x4_t));
	static_assert(sizeof(VMatrix) >= sizeof(matrix3x4_t));
	return *reinterpret_cast<matrix3x4_t*>(this);
}

inline void VMatrix::CopyFrom3x4( const matrix3x4_t &m3x4 )
{
	memcpy( m, m3x4.Base(), sizeof( matrix3x4_t ) ); //-V512 //-V1086
	m[3][0] = m[3][1] = m[3][2] = 0;
	m[3][3] = 1;
}

inline void	VMatrix::Set3x4( matrix3x4_t& matrix3x4 ) const
{
	memcpy(matrix3x4.Base(), m, sizeof( matrix3x4_t ) ); //-V512 //-V1086
}


//-----------------------------------------------------------------------------
// Matrix math operations
//-----------------------------------------------------------------------------
inline const VMatrix& VMatrix::operator+=(const VMatrix &other)
{
	DirectX::XMStoreFloat4x4
	(
		XmMBase(),
		DirectX::XMLoadFloat4x4( XmMBase() ) + DirectX::XMLoadFloat4x4( other.XmMBase() )
	);
	return *this;
}


#ifndef VECTOR_NO_SLOW_OPERATIONS

inline VMatrix VMatrix::operator+(const VMatrix &other) const
{
	VMatrix ret;
	DirectX::XMStoreFloat4x4
	(
		ret.XmMBase(),
		DirectX::XMLoadFloat4x4( XmMBase() ) + DirectX::XMLoadFloat4x4( other.XmMBase() )
	);
	return ret;
}

inline VMatrix VMatrix::operator-(const VMatrix &other) const
{
	VMatrix ret;
	DirectX::XMStoreFloat4x4
	(
		ret.XmMBase(),
		DirectX::XMLoadFloat4x4( XmMBase() ) - DirectX::XMLoadFloat4x4( other.XmMBase() )
	);
	return ret;
}

inline VMatrix VMatrix::operator-() const
{
	VMatrix ret;
	for( int i=0; i < 16; i++ )
	{
		((float*)ret.m)[i] = ((const float*)m)[i];
	}
	return ret;
}

#endif // VECTOR_NO_SLOW_OPERATIONS


//-----------------------------------------------------------------------------
// Vector transformation
//-----------------------------------------------------------------------------

#ifndef VECTOR_NO_SLOW_OPERATIONS

inline Vector VMatrix::operator*(const Vector &vVec) const
{
	Vector vRet;
	vRet.x = m[0][0]*vVec.x + m[0][1]*vVec.y + m[0][2]*vVec.z + m[0][3];
	vRet.y = m[1][0]*vVec.x + m[1][1]*vVec.y + m[1][2]*vVec.z + m[1][3];
	vRet.z = m[2][0]*vVec.x + m[2][1]*vVec.y + m[2][2]*vVec.z + m[2][3];

	return vRet;
}

inline Vector VMatrix::VMul4x3(const Vector &vVec) const
{
	Vector vResult;
	Vector3DMultiplyPosition( *this, vVec, vResult );
	return vResult;
}


inline Vector VMatrix::VMul4x3Transpose(const Vector &vVec) const
{
	Vector tmp = vVec;
	tmp.x -= m[0][3];
	tmp.y -= m[1][3];
	tmp.z -= m[2][3];

	return Vector(
		m[0][0]*tmp.x + m[1][0]*tmp.y + m[2][0]*tmp.z,
		m[0][1]*tmp.x + m[1][1]*tmp.y + m[2][1]*tmp.z,
		m[0][2]*tmp.x + m[1][2]*tmp.y + m[2][2]*tmp.z
		);
}

inline Vector VMatrix::VMul3x3(const Vector &vVec) const
{
	return Vector(
		m[0][0]*vVec.x + m[0][1]*vVec.y + m[0][2]*vVec.z,
		m[1][0]*vVec.x + m[1][1]*vVec.y + m[1][2]*vVec.z,
		m[2][0]*vVec.x + m[2][1]*vVec.y + m[2][2]*vVec.z
		);
}

inline Vector VMatrix::VMul3x3Transpose(const Vector &vVec) const
{
	return Vector(
		m[0][0]*vVec.x + m[1][0]*vVec.y + m[2][0]*vVec.z,
		m[0][1]*vVec.x + m[1][1]*vVec.y + m[2][1]*vVec.z,
		m[0][2]*vVec.x + m[1][2]*vVec.y + m[2][2]*vVec.z
		);
}

#endif // VECTOR_NO_SLOW_OPERATIONS


inline void VMatrix::V3Mul(const Vector &vIn, Vector &vOut) const
{
	DirectX::XMVECTOR vvin = DirectX::XMVectorSetW( DirectX::XMLoadFloat3( vIn.XmBase() ), 1.0f ); //-V2002
	DirectX::XMVECTOR vden = DirectX::XMVectorSum
	(
		DirectX::XMVectorMultiply( DirectX::XMLoadFloat4( XmBase() + 3 ), vvin )
	);
	DirectX::XMVECTOR vrw  = DirectX::XMVectorDivide( DirectX::g_XMOne, vden );

	DirectX::XMStoreFloat3
	(
		vOut.XmBase(),
		DirectX::XMVectorSet
		(
			DirectX::XMVectorGetX //-V2002
			(
				DirectX::XMVectorMultiply
				(
					DirectX::XMVectorSum( DirectX::XMVectorMultiply( DirectX::XMLoadFloat4( XmBase() ), vvin ) ),
					vrw
				)
			),
			DirectX::XMVectorGetX //-V2002
			(
				DirectX::XMVectorMultiply
				(
					DirectX::XMVectorSum( DirectX::XMVectorMultiply( DirectX::XMLoadFloat4( XmBase() + 1 ), vvin ) ),
					vrw
				)
			),
			DirectX::XMVectorGetX //-V2002
			(
				DirectX::XMVectorMultiply
				(
					DirectX::XMVectorSum( DirectX::XMVectorMultiply( DirectX::XMLoadFloat4( XmBase() + 2 ), vvin ) ),
					vrw
				)
			),
			0.0f
		)
	);
}

inline void VMatrix::V4Mul(const Vector4D &vIn, Vector4D &vOut) const
{
	DirectX::XMVECTOR vvin = DirectX::XMLoadFloat4( vIn.XmBase() );

	DirectX::XMStoreFloat4
	(
		vOut.XmBase(),
		DirectX::XMVectorSet
		(
			DirectX::XMVectorGetX //-V2002
			(
				DirectX::XMVectorSum
				(
					DirectX::XMVectorMultiply( DirectX::XMLoadFloat4( XmBase() ), vvin )
				)
			),
			DirectX::XMVectorGetX //-V2002
			(
				DirectX::XMVectorSum
				(
					DirectX::XMVectorMultiply( DirectX::XMLoadFloat4( XmBase() + 1 ), vvin )
				)
			),
			DirectX::XMVectorGetX //-V2002
			(
				DirectX::XMVectorSum
				(
					DirectX::XMVectorMultiply( DirectX::XMLoadFloat4( XmBase() + 2 ), vvin )
				)
			),
			DirectX::XMVectorGetX //-V2002
			(
				DirectX::XMVectorSum
				(
					DirectX::XMVectorMultiply( DirectX::XMLoadFloat4( XmBase() + 3 ), vvin )
				)
			)
		)
	);
}


//-----------------------------------------------------------------------------
// Plane transformation
//-----------------------------------------------------------------------------
inline void	VMatrix::TransformPlane( const VPlane &inPlane, VPlane &outPlane ) const
{
	Vector vTrans;
	Vector3DMultiply( *this, inPlane.m_Normal, outPlane.m_Normal );
	outPlane.m_Dist = inPlane.m_Dist * DotProduct( outPlane.m_Normal, outPlane.m_Normal );
	outPlane.m_Dist += DotProduct( outPlane.m_Normal, GetTranslation( vTrans ) );
}


//-----------------------------------------------------------------------------
// Other random stuff
//-----------------------------------------------------------------------------
inline void VMatrix::Identity()
{
	MatrixSetIdentity( *this );
}


inline bool VMatrix::IsIdentity() const
{
	return DirectX::XMMatrixIsIdentity( DirectX::XMLoadFloat4x4( XmMBase() ) );
}

#ifndef VECTOR_NO_SLOW_OPERATIONS

inline Vector VMatrix::ApplyRotation(const Vector &vVec) const
{
	return VMul3x3(vVec);
}

inline VMatrix VMatrix::operator~() const
{
	VMatrix mRet;
	[[maybe_unused]] bool ok = InverseGeneral(mRet);
	AssertMsg( ok, "Matrix inverse failed." );
	return mRet;
}

#endif


//-----------------------------------------------------------------------------
// Accessors
//-----------------------------------------------------------------------------
inline void MatrixGetColumn( const VMatrix &src, int nCol, Vector *pColumn )
{
	Assert( (nCol >= 0) && (nCol <= 3) );

	pColumn->x = src[0][nCol]; //-V108
	pColumn->y = src[1][nCol]; //-V108
	pColumn->z = src[2][nCol]; //-V108
}

inline void MatrixSetColumn( VMatrix &src, int nCol, const Vector &column )
{
	Assert( (nCol >= 0) && (nCol <= 3) );

	src.m[0][nCol] = column.x;
	src.m[1][nCol] = column.y;
	src.m[2][nCol] = column.z;
}

inline void MatrixGetRow( const VMatrix &src, int nRow, Vector *pRow )
{
	Assert( (nRow >= 0) && (nRow <= 3) );
	*pRow = *(const Vector*)src[nRow];
}

inline void MatrixSetRow( VMatrix &dst, int nRow, const Vector &row )
{
	Assert( (nRow >= 0) && (nRow <= 3) );
	*(Vector*)dst[nRow] = row;
}


//-----------------------------------------------------------------------------
// Vector3DMultiplyPosition treats src2 as if it's a point (adds the translation)
//-----------------------------------------------------------------------------
// NJS: src2 is passed in as a full vector rather than a reference to prevent the need
// for 2 branches and a potential copy in the body.  (ie, handling the case when the src2
// reference is the same as the dst reference ).
inline void Vector3DMultiplyPosition( const VMatrix& src1, const VectorByValue src2, Vector& dst )
{
	dst[0] = src1[0][0] * src2.x + src1[0][1] * src2.y + src1[0][2] * src2.z + src1[0][3];
	dst[1] = src1[1][0] * src2.x + src1[1][1] * src2.y + src1[1][2] * src2.z + src1[1][3];
	dst[2] = src1[2][0] * src2.x + src1[2][1] * src2.y + src1[2][2] * src2.z + src1[2][3];
}


//-----------------------------------------------------------------------------
// Transform a plane that has an axis-aligned normal
//-----------------------------------------------------------------------------
inline void MatrixTransformAxisAlignedPlane( const VMatrix &src, int nDim, float flSign, float flDist, cplane_t &outPlane )
{
	// See MatrixTransformPlane in the .cpp file for an explanation of the algorithm.
	MatrixGetColumn( src, nDim, &outPlane.normal );
	outPlane.normal *= flSign;
	outPlane.dist = flDist * DotProduct( outPlane.normal, outPlane.normal );

	// NOTE: Writing this out by hand because it doesn't inline (inline depth isn't large enough)
	// This should read outPlane.dist += DotProduct( outPlane.normal, src.GetTranslation );
	outPlane.dist += outPlane.normal.x * src.m[0][3] + outPlane.normal.y * src.m[1][3] + outPlane.normal.z * src.m[2][3];
}


//-----------------------------------------------------------------------------
// Matrix equality test
//-----------------------------------------------------------------------------
[[nodiscard]] inline bool MatricesAreEqual( const VMatrix &src1, const VMatrix &src2, float flTolerance )
{
	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 3; ++j )
		{
			if ( fabsf( src1[i][j] - src2[i][j] ) > flTolerance )
				return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void MatrixBuildOrtho( VMatrix& dst, double left, double top, double right, double bottom, double zNear, double zFar );
void MatrixBuildPerspectiveX( VMatrix& dst, float flFovX, float flAspect, float flZNear, float flZFar );
void MatrixBuildPerspectiveOffCenterX( VMatrix& dst, double flFovX, double flAspect, double flZNear, double flZFar, double bottom, double top, double left, double right );
void MatrixBuildPerspectiveZRange( VMatrix& dst, float flZNear, float flZFar );

inline void MatrixOrtho( VMatrix& dst, double left, double top, double right, double bottom, double zNear, double zFar )
{
	VMatrix mat;
	MatrixBuildOrtho( mat, left, top, right, bottom, zNear, zFar );

	VMatrix temp;
	MatrixMultiply( dst, mat, temp );
	dst = temp;
}

inline void MatrixPerspectiveX( VMatrix& dst, float flFovX, float flAspect, float flZNear, float flZFar )
{
	VMatrix mat;
	MatrixBuildPerspectiveX( mat, flFovX, flAspect, flZNear, flZFar );

	VMatrix temp;
	MatrixMultiply( dst, mat, temp );
	dst = temp;
}

inline void MatrixPerspectiveOffCenterX( VMatrix& dst, double flFovX, double flAspect, double flZNear, double flZFar, double bottom, double top, double left, double right )
{
	VMatrix mat;
	MatrixBuildPerspectiveOffCenterX( mat, flFovX, flAspect, flZNear, flZFar, bottom, top, left, right );

	VMatrix temp;
	MatrixMultiply( dst, mat, temp );
	dst = temp;
}

#endif


