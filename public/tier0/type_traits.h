//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: functionality that is provided in C++11 type_traits, which 
// currently isn't supported by the OSX compiler. Sadness.
//
//=============================================================================
#ifndef SE_PUBLIC_TIER0_TYPE_TRAITS_H_
#define SE_PUBLIC_TIER0_TYPE_TRAITS_H_

template <typename T>
struct V_remove_const
{
	using type = T;
};

template <typename T>
struct V_remove_const<const T>
{
	using type = T;
};

template <typename T>
struct V_remove_const<const T[]>
{
	using type = T;
};

template <typename T, unsigned int N>
struct V_remove_const<const T[N]>
{
	using type = T;
};

#endif  // !SE_PUBLIC_TIER0_TYPE_TRAITS_H_
