//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef MINMAX_H
#define MINMAX_H

// On older GCC #include <algorithm> gets obliterated by our silly -Dfopen=dont_use_fopen define. Since the other
// buildbots will fail if someone does use fopen, I'm partially turning off the safety here to fix this.

// If you're getting some weird fopen error from this, it's because someone included stdio before this file. Usually
// moving the basetypes.h include higher in the file that is exploding fixes this.
#if defined( fopen ) && defined( __GNUC__ ) && __GNUC__ < 5
#undef fopen
#endif

#include <algorithm>

using std::min;
using std::max;

#endif // MINMAX_H
