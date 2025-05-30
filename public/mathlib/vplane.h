//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef VPLANE_H
#define VPLANE_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"

// Used to represent sides of things like planes.
enum SideType : int
{
	SIDE_FRONT = 0,
	SIDE_BACK = 1,
	SIDE_ON = 2,
	SIDE_CROSS = -2  // necessary for polylib.cpp
};

constexpr inline vec_t VP_EPSILON{0.01f};

// Plane.
class VPlane
{
public:
	VPlane() : m_Normal(), m_Dist(0) {}
	VPlane(const Vector &vNormal, vec_t dist);

	void Init(const Vector &vNormal, vec_t dist);

	// Return the distance from the point to the plane.
	[[nodiscard]] vec_t DistTo(const Vector &vVec) const;

	// Copy.
	VPlane& operator=(const VPlane &thePlane);
	VPlane(const VPlane &thePlane);

	// Returns SIDE_ON, SIDE_FRONT, or SIDE_BACK.
	// The epsilon for SIDE_ON can be passed in.
	[[nodiscard]] SideType	GetPointSide(const Vector &vPoint, vec_t sideEpsilon=VP_EPSILON) const;

	// Returns SIDE_FRONT or SIDE_BACK.
	[[nodiscard]] SideType	GetPointSideExact(const Vector &vPoint) const;

	// Classify the box with respect to the plane.
	// Returns SIDE_ON, SIDE_FRONT, or SIDE_BACK
	[[nodiscard]] SideType	BoxOnPlaneSide(const Vector &vMin, const Vector &vMax) const;

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// Flip the plane.
	[[nodiscard]] VPlane		Flip();

	// Get a point on the plane (normal*dist).
	[[nodiscard]] Vector		GetPointOnPlane() const;

	// Snap the specified point to the plane (along the plane's normal).
	[[nodiscard]] Vector		SnapPointToPlane(const Vector &vPoint) const;
#endif

public:
	Vector		m_Normal;
	vec_t		m_Dist;

#ifdef VECTOR_NO_SLOW_OPERATIONS
private:
	// No copy constructors allowed if we're in optimal mode
	VPlane(const VPlane&) = delete;
#endif
};


//-----------------------------------------------------------------------------
// Inlines.
//-----------------------------------------------------------------------------
inline VPlane::VPlane(const Vector &vNormal, vec_t dist)
    : m_Normal{vNormal}, m_Dist{dist}
{
}

inline void	VPlane::Init(const Vector &vNormal, vec_t dist)
{
	m_Normal = vNormal;
	m_Dist = dist;
}

inline vec_t VPlane::DistTo(const Vector &vVec) const
{
	return vVec.Dot(m_Normal) - m_Dist;
}

inline VPlane& VPlane::operator=(const VPlane &thePlane)
{
	m_Normal = thePlane.m_Normal;
	m_Dist = thePlane.m_Dist;
	return *this;
}

inline VPlane::VPlane(const VPlane &thePlane)
	: m_Normal{thePlane.m_Normal}, m_Dist{thePlane.m_Dist}
{
}

#ifndef VECTOR_NO_SLOW_OPERATIONS

inline VPlane VPlane::Flip()
{
	return VPlane(-m_Normal, -m_Dist);
}

inline Vector VPlane::GetPointOnPlane() const
{
	return m_Normal * m_Dist;
}

inline Vector VPlane::SnapPointToPlane(const Vector &vPoint) const
{
	return vPoint - m_Normal * DistTo(vPoint);
}

#endif

inline SideType VPlane::GetPointSide(const Vector &vPoint, vec_t sideEpsilon) const
{
	vec_t fDist = DistTo(vPoint);
	if (fDist >= sideEpsilon)
		return SIDE_FRONT;
	if (fDist <= -sideEpsilon)
		return SIDE_BACK;
	return SIDE_ON;
}

inline SideType VPlane::GetPointSideExact(const Vector &vPoint) const
{
	return DistTo(vPoint) > 0.0f ? SIDE_FRONT : SIDE_BACK;
}


// BUGBUG: This should either simply use the implementation in mathlib or cease to exist.
// mathlib implementation is much more efficient.  Check to see that VPlane isn't used in
// performance critical code.
inline SideType VPlane::BoxOnPlaneSide(const Vector &vMin, const Vector &vMax) const
{
	TableVector vPoints[8] = 
	{
		{ vMin.x, vMin.y, vMin.z },
		{ vMin.x, vMin.y, vMax.z },
		{ vMin.x, vMax.y, vMax.z },
		{ vMin.x, vMax.y, vMin.z },

		{ vMax.x, vMin.y, vMin.z },
		{ vMax.x, vMin.y, vMax.z },
		{ vMax.x, vMax.y, vMax.z },
		{ vMax.x, vMax.y, vMin.z },
	};

	SideType firstSide = GetPointSideExact(vPoints[0]);
	for (const auto &p : vPoints)
	{
		SideType side = GetPointSideExact(p);

		// Does the box cross the plane?
		if (side != firstSide)
			return SIDE_ON;
	}

	// Ok, they're all on the same side, return that.
	return firstSide;
}




#endif // VPLANE_H
