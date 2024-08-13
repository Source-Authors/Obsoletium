// Copyright Valve Corporation, All rights reserved.

#ifndef CRTMEMDEBUG_H
#define CRTMEMDEBUG_H
#pragma once

#ifdef USECRTMEMDEBUG
#include <crtdbg.h>
#define MEMCHECK CheckHeap()
void CheckHeap();
#else
#define MEMCHECK
#endif

int InitCRTMemDebug();
void ShutdownCRTMemDebug(int prevState);

#endif  // CRTMEMDEBUG_H
