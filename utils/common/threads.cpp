// Copyright Valve Corporation, All rights reserved.
//
//

#define NO_THREAD_NAMES
#include "threads.h"

#include "cmdlib.h"
#include "pacifier.h"
#include "winlite.h"

#include "vstdlib/jobthread.h"

namespace {

struct RunThreadArgs {
  int thread_no;
  void *user_data;
  RunThreadsFn run_func;
};

RunThreadArgs g_RunThreadArgs[MAX_THREADS];

int dispatch;
int workcount;
bool pacifier;

bool enable_threads;
bool g_bLowPriorityThreads = false;

HANDLE g_ThreadHandles[MAX_THREADS];

class ScopedCriticalSectionLock {
 public:
  explicit ScopedCriticalSectionLock(CRITICAL_SECTION &csi) noexcept
      : cs_{csi} {
    EnterCriticalSection(&csi);
  }
  ~ScopedCriticalSectionLock() noexcept { LeaveCriticalSection(&cs_); }

  ScopedCriticalSectionLock(ScopedCriticalSectionLock &) = delete;
  ScopedCriticalSectionLock &operator=(ScopedCriticalSectionLock &) = delete;

 private:
  CRITICAL_SECTION &cs_;
};

class ScopedCriticalSection {
 public:
  explicit ScopedCriticalSection(unsigned int spin_count) noexcept {
    // Starting with Windows Vista, the InitializeCriticalSectionAndSpinCount
    // function always succeeds, even in low memory situations.
    (void)InitializeCriticalSectionAndSpinCount(&cs_, spin_count);
  }
  ~ScopedCriticalSection() noexcept { DeleteCriticalSection(&cs_); }

  [[nodiscard]] ScopedCriticalSectionLock Lock() noexcept {
    return ScopedCriticalSectionLock{cs_};
  }

  ScopedCriticalSection(ScopedCriticalSection &) = delete;
  ScopedCriticalSection &operator=(ScopedCriticalSection &) = delete;

 private:
  CRITICAL_SECTION cs_;
};

ScopedCriticalSection g_threads_critical_section{2000};

ThreadWorkerFn worker;

int GetThreadWork() {
  const ScopedCriticalSectionLock lock{g_threads_critical_section.Lock()};

  if (dispatch == workcount) return -1;

  UpdatePacifier((float)dispatch / workcount);

  int r = dispatch;
  ++dispatch;

  return r;
}

void ThreadWorker(int iThread, void *pUserData) {
  while (true) {
    int work = GetThreadWork();

    if (work == -1) break;

    worker(iThread, work);
  }
}

}  // namespace

void RunThreadsOnIndividual(int workcnt, qboolean showpacifier,
                            ThreadWorkerFn func) {
  if (numthreads == -1) ThreadSetDefault();

  worker = func;

  RunThreadsOn(workcnt, showpacifier, ThreadWorker);
}

int numthreads = -1;

void SetLowPriority() {
  SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
}

void ThreadSetDefault() {
  // not set manually
  if (numthreads == -1) {
    SYSTEM_INFO info;
    GetNativeSystemInfo(&info);

    numthreads = info.dwNumberOfProcessors;

    if (numthreads < 1 || numthreads > MAX_THREADS) numthreads = 1;
  }

  Msg("%i threads\n", numthreads);
}

// This runs in the thread and dispatches a RunThreadsFn call.
static DWORD WINAPI InternalRunThreadsFn(void *arg) {
  auto *args = static_cast<RunThreadArgs *>(arg);

  args->run_func(args->thread_no, args->user_data);

  return 0;
}

void RunThreads_Start(RunThreadsFn fn, void *pUserData,
                      ERunThreadsPriority ePriority) {
  Assert(numthreads > 0);

  enable_threads = true;

  if (numthreads > MAX_TOOL_THREADS) numthreads = MAX_TOOL_THREADS;

  for (int i = 0; i < numthreads; i++) {
    RunThreadArgs &args = g_RunThreadArgs[i];
    args.thread_no = i;
    args.user_data = pUserData;
    args.run_func = fn;

    DWORD dwDummy;
    HANDLE thread{
        CreateThread(NULL, 0, InternalRunThreadsFn, &args, 0, &dwDummy)};
    if (!thread) continue;

    switch (ePriority) {
      case ERunThreadsPriority::k_eRunThreadsPriority_UseGlobalState:
        if (g_bLowPriorityThreads)
          SetThreadPriority(thread, THREAD_PRIORITY_LOWEST);
        break;

      case ERunThreadsPriority::k_eRunThreadsPriority_Normal:
        break;

      case ERunThreadsPriority::k_eRunThreadsPriority_Idle:
        SetThreadPriority(thread, THREAD_PRIORITY_IDLE);
        break;

      default:
        break;
    }

    g_ThreadHandles[i] = thread;
  }
}

void RunThreads_End() {
  WaitForMultipleObjects(numthreads, g_ThreadHandles, TRUE, INFINITE);

  for (int i = 0; i < numthreads; i++) {
    if (g_ThreadHandles[i]) CloseHandle(g_ThreadHandles[i]);
  }

  enable_threads = false;
}

void RunThreadsOn(int workcnt, qboolean showpacifier, RunThreadsFn fn,
                  void *user_data) {
  const double start{Plat_FloatTime()};

  dispatch = 0;
  workcount = workcnt;
  StartPacifier("");
  pacifier = showpacifier;

#ifdef _PROFILE
  threaded = false;
  (*func)(0);
  return;
#endif

  RunThreads_Start(fn, user_data);
  RunThreads_End();

  const double end{Plat_FloatTime()};

  if (pacifier) {
    EndPacifier(false);
    printf(" (%.2f)\n", end - start);
  }
}
