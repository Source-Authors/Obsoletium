// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_COMMON_THREADS_H_
#define SRC_UTILS_COMMON_THREADS_H_

#include "vstdlib/jobthread.h"

// Arrays that are indexed by thread should always be MAX_TOOL_THREADS+1
// large so THREADINDEX_MAIN can be used from the main thread.
constexpr inline int MAX_TOOL_THREADS{MAX_THREADS};
constexpr inline int THREADINDEX_MAIN{MAX_TOOL_THREADS};

extern int numthreads;

// If set to true, then all the threads that are created are low priority.
// extern bool g_bLowPriorityThreads;

using ThreadWorkerFn = void (*)(int iThread, int iWorkItem);
using RunThreadsFn = void (*)(int iThread, void *pUserData);

enum class ERunThreadsPriority {
  // Default.. uses g_bLowPriorityThreads to decide what to set the
  // priority to.
  k_eRunThreadsPriority_UseGlobalState = 0,
  // Doesn't touch thread priorities.
  k_eRunThreadsPriority_Normal,
  // Sets threads to idle priority.
  k_eRunThreadsPriority_Idle
};

// Put the process into an idle priority class so it doesn't hog the UI.
void SetLowPriority();

void ThreadSetDefault();

void RunThreadsOnIndividual(int workcnt, qboolean showpacifier,
                            ThreadWorkerFn fn);

void RunThreadsOn(int workcnt, qboolean showpacifier, RunThreadsFn fn,
                  void *pUserData = nullptr);

// This version doesn't track work items - it just runs your function and waits
// for it to finish.
void RunThreads_Start(
    RunThreadsFn func, void *user_data,
    ERunThreadsPriority priority =
        ERunThreadsPriority::k_eRunThreadsPriority_UseGlobalState);
void RunThreads_End();

#ifndef NO_THREAD_NAMES
#define RunThreadsOn(n, p, f)        \
  {                                  \
    if (p) printf("%-20s ", #f ":"); \
    RunThreadsOn(n, p, f);           \
  }
#define RunThreadsOnIndividual(n, p, f) \
  {                                     \
    if (p) printf("%-20s ", #f ":");    \
    RunThreadsOnIndividual(n, p, f);    \
  }
#endif

#endif  // !SRC_UTILS_COMMON_THREADS_H_
