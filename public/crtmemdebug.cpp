// Copyright Valve Corporation, All rights reserved.

#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)

#include "crtmemdebug.h"

#ifdef USECRTMEMDEBUG
#include <crtdbg.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CheckHeap() {
#ifdef USECRTMEMDEBUG
  _CrtCheckMemory();
#endif
}

// TODO: This needs to be somehow made to construct before everything else.
int InitCRTMemDebug() {
#ifdef USECRTMEMDEBUG
  int prevState = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);

  _CrtSetDbgFlag(prevState |
                 //	_	CRTDBG_ALLOC_MEM_DF  |
                 _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_CHECK_CRT_DF |
                 _CRTDBG_DELAY_FREE_MEM_DF);

  return prevState;
#else
  return 0;
#endif
}

void ShutdownCRTMemDebug([[maybe_unused]] int prevState) {
#ifdef USECRTMEMDEBUG
  _CrtSetDbgFlag(prevState);
#endif
}

#endif  // !_STATIC_LINKED || _SHARED_LIB
