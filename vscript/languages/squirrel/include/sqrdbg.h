/*
Copyright (c) 2003-2005 Alberto Demichelis

This software is provided 'as-is', without any express or implied warranty. In
no event will the authors be held liable for any damages arising from the use of
this software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject to
the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
that you wrote the original software. If you use this software in a product, an
acknowledgment in the product documentation would be appreciated but is
not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.
*/

// Altered version.

#ifndef SQ_INCLUDE_SQRDBG_H_
#define SQ_INCLUDE_SQRDBG_H_

#include <cstdlib>  // std::size_t

#ifdef _WIN32
#pragma comment(lib, "WSOCK32.LIB")
#endif

#if (defined(_WIN64) || defined(_LP64))
#ifndef _SQ64
#define _SQ64
#endif
#endif

#ifdef _SQ64
#ifdef _MSC_VER
using SQInteger = __int64;
#else
using SQInteger = long long;
#endif
#else
using SQInteger = int;
#endif

using HSQUIRRELVM = struct SQVM*;
using SQRESULT = SQInteger;
using SQBool = std::size_t;

namespace sq::dbg {

struct SQDbgServer;
using HSQREMOTEDBG = SQDbgServer*;

HSQREMOTEDBG sq_rdbg_init(HSQUIRRELVM v, unsigned short port,
                          SQBool autoupdate);
SQRESULT sq_rdbg_shutdown(HSQREMOTEDBG* rdbg);

SQRESULT sq_rdbg_waitforconnections(HSQREMOTEDBG rdbg);
SQRESULT sq_rdbg_update(HSQREMOTEDBG rdbg);

}  // namespace sq::dbg

#endif  // !SQ_INCLUDE_SQRDBG_H_
