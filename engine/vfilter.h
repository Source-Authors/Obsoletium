// Copyright Valve Corporation, All rights reserved.
//

#ifndef SE_ENGINE_VFILTER_H_
#define SE_ENGINE_VFILTER_H_

#include "userid.h"

struct ipfilter_t
{
	unsigned	mask;
	unsigned	compare;
	// dimhotepus: float -> double for better precision.
	double       banEndTime; // 0 for permanent ban
	// dimhotepus: float -> double for better precision.
	double       banTime;
};

struct userfilter_t
{
	USERID_t userid;
	// dimhotepus: float -> double for better precision.
	double	banEndTime;
	// dimhotepus: float -> double for better precision.
	double	banTime;
};

constexpr inline intp MAX_IPFILTERS{32768};
constexpr inline intp MAX_USERFILTERS{32768};

#endif  // !SE_ENGINE_VFILTER_H_
