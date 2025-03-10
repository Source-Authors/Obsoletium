// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"
#include "tier0_strtools.h"

[[nodiscard]] static constexpr inline int TOLOWERC( int x )
{
	return x >= 'A' && x <= 'Z' ? x + 32 : x;
}

extern "C"
[[nodiscard]] int V_tier0_stricmp( IN_Z const char *s1, IN_Z const char *s2 )
{
	if (s1 == s2) return 0;

	uint8 const *pS1 = ( uint8 const * ) s1;
	uint8 const *pS2 = ( uint8 const * ) s2;
	for(;;)
	{
		{
			const int c1 = *(pS1++), c2 = *(pS2++);
			if (c1 == c2)
			{
				if (!c1) return 0;
			}
			else
			{
				if (!c2)
				{
					return c1 - c2;
				}

				const int cc1 = TOLOWERC(c1), cc2 = TOLOWERC(c2);
				if (cc1 != cc2)
				{
					return cc1 - cc2;
				}
			}
		}

		{
			const int c1 = *(pS1++), c2 = *(pS2++);
			if (c1 == c2)
			{
				if (!c1) return 0;
			}
			else
			{
				if (!c2)
				{
					return c1 - c2;
				}

				const int cc1 = TOLOWERC(c1), cc2 = TOLOWERC(c2);
				if (cc1 != cc2)
				{
					return cc1 - cc2;
				}
			}
		}
	}
}
