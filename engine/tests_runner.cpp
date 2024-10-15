// Copyright Valve Corporation, All rights reserved.
//
// Self-tests commands.

#include "tests_thread_pool.h"
#include "tests_ts_collections.h"

#include <atomic>

#include "tier0/threadtools.h"
#include "tier1/convar.h"

#include "vstdlib/jobthread.h"

// Should be last header.
#include "tier0/memdbgon.h"

namespace {

CThreadEvent g_release_thread_reservation_event{true};
std::atomic_int g_thread_pool_reserved_threads_num;

void ThreadPoolThreadReserver() {
  g_release_thread_reservation_event.Wait();

  g_thread_pool_reserved_threads_num.fetch_sub(1, std::memory_order_relaxed);
}

void ReserveThreadPoolThreads(intp threads_to_reserve_num) {
  threads_to_reserve_num =
      std::clamp(threads_to_reserve_num, static_cast<intp>(0),
                 g_pThreadPool->NumThreads());

  g_release_thread_reservation_event.Set();
  while (g_thread_pool_reserved_threads_num.load(std::memory_order_relaxed) !=
         0) {
    ThreadSleep(0);
  }
  g_release_thread_reservation_event.Reset();

  while (threads_to_reserve_num--) {
    g_thread_pool_reserved_threads_num.fetch_add(1, std::memory_order_relaxed);

    g_pThreadPool->QueueCall(&ThreadPoolThreadReserver)->Release();
  }

  Msg("Global thread pool %d threads being reserved.\n",
      g_thread_pool_reserved_threads_num.load(std::memory_order_relaxed));
}

}  // namespace

void OnChangeThreadPoolReserve(IConVar *, const char *, float);

ConVar threadpool_reserve(
    "threadpool_reserve", "0", 0,
    "Consume the specified number of threads in the thread pool.", 0, 0, 0, 0,
    &OnChangeThreadPoolReserve);

void OnChangeThreadPoolReserve(IConVar *, const char *, float) {
  ReserveThreadPoolThreads(threadpool_reserve.GetInt());
}

CON_COMMAND(threadpool_cycle_reserve,
            "Cycles threadpool reservation by powers of 2.") {
  const intp max_threads_num{g_pThreadPool->NumThreads() + 1};
  const intp available_threads_num{
      max_threads_num -
      g_thread_pool_reserved_threads_num.load(std::memory_order_relaxed)};

  Assert(available_threads_num);

  intp ratio{max_threads_num / available_threads_num};
  ratio *= 2;

  if (ratio > max_threads_num) {
    ReserveThreadPoolThreads(0);
  } else {
    ReserveThreadPoolThreads(max_threads_num - max_threads_num / ratio);
  }
}

CON_COMMAND(thread_test_tslist,
            "Run thread-safe list tests. 10000 list size by default.") {
  const int tests_num{args.ArgC() == 1 ? 10000 : atoi(args.Arg(1))};

  se::engine::tests::ts_collections::RunTSListTests(tests_num);
}

CON_COMMAND(thread_test_tsqueue,
            "Run thread-safe queue tests. 10000 queue size by default.") {
  const int tests_num{args.ArgC() == 1 ? 10000 : atoi(args.Arg(1))};

  se::engine::tests::ts_collections::RunTSQueueTests(tests_num);
}

CON_COMMAND(threadpool_run_tests,
            "Run thread pool tests. 1 test run by default.") {
  const int tests_num{args.ArgC() == 1 ? 1 : atoi(args.Arg(1))};

  for (int i = 0; i < tests_num; i++) {
    se::engine::tests::thread_pool::RunThreadPoolTests();
  }
}
