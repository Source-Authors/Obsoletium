// Copyright Valve Corporation, All rights reserved.
//

#ifndef SE_ENGINE_VFILTER_H_
#define SE_ENGINE_VFILTER_H_

#include "userid.h"

struct ipfilter_t
{
	unsigned	mask;
	unsigned	compare;
	float       banEndTime; // 0 for permanent ban
	float       banTime;
};

struct userfilter_t
{
	USERID_t userid;
	float	banEndTime;
	float	banTime;
};

constexpr inline intp MAX_IPFILTERS{32768};
constexpr inline intp MAX_USERFILTERS{32768};

#endif  // !SE_ENGINE_VFILTER_H_
