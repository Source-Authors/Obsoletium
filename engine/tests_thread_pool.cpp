// Copyright Valve Corporation, All rights reserved.
//
// Thread pool self-tests.

#include "tests_ts_collections.h"

#include <atomic>
#include <cmath>
#include <limits>
#include <memory>

#include "tier0/dbg.h"
#include "tier0/fasttimer.h"
#include "tier0/threadtools.h"
#include "tier1/generichash.h"
#include "vstdlib/jobthread.h"

#ifdef PLATFORM_WINDOWS_PC
#include "winlite.h"
#endif

#include "tier0/memdbgon.h"

namespace {

constexpr int kJobsNum{4000};

// Failed checks across all tests.
std::atomic_int g_failures_num{0};

#define TP_TEST_EXPECT(cond, ...)                         \
  do {                                                    \
    if (!(cond)) {                                        \
      g_failures_num.fetch_add(1, std::memory_order_relaxed); \
      Warning("RunThreadPoolTests: FAILED: " __VA_ARGS__); \
      AssertMsg(false, "Thread pool test failed: %s", #cond); \
    }                                                     \
  } while (0)

// Some CPU work without shared state. RandomInt() goes through a mutex
// protected global stream, so using it here measured lock contention instead.
double DoSomeWork(uint32 seed) {
  byte memory[1024];

  uint32 state{seed | 1u};
  for (auto &b : memory) {
    // xorshift32
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    b = static_cast<byte>(state);
  }

  double acc{0.0};
  for (size_t i = 0; i < 50; i++) {
    acc += sqrt(HashBlock(memory, std::size(memory)) +
                HashBlock(memory, std::size(memory)) + 10.0);
  }
  return acc;
}

uint32 SeedFrom(const void *p) {
  return static_cast<uint32>(reinterpret_cast<uintptr_t>(p) >> 4);
}

class CountJob : public CJob {
 public:
  CountJob(CThreadEvent &done_event, unsigned sleep_ms, int total_count,
           bool should_do_work)
      : done_event_{done_event},
        sleep_ms_{sleep_ms},
        total_count_{total_count},
        should_do_work_{should_do_work} {}

  JobStatus_t DoExecute() override {
    executions_num_.fetch_add(1, std::memory_order_relaxed);

    ThreadPause();

    ThreadSleep(sleep_ms_);

    if (should_do_work_) {
      should_do_work_ = DoSomeWork(SeedFrom(this)) != 0.0;
    }

    // Count when job is done, not when it starts: last finished job signals,
    // exactly once.
    if (m_nCount.fetch_add(1, std::memory_order_acq_rel) + 1 ==
        total_count_) {
      done_event_.Set();
    }

    return JOB_OK;
  }

  [[nodiscard]] int GetExecutionsNum() const {
    return executions_num_.load(std::memory_order_relaxed);
  }

  static std::atomic_int m_nCount;

 private:
  CThreadEvent &done_event_;
  const unsigned sleep_ms_;
  const int total_count_;
  bool should_do_work_;
  std::atomic_int executions_num_{0};
};

std::atomic_int CountJob::m_nCount{0};

double SafeDiv(double a, double b) { return b != 0.0 ? a / b : 0.0; }

void Test(IThreadPool *pool, bool should_distribute, bool should_sleep = true,
          bool should_finish = false, bool do_work = false) {
  const int max_threads_num{GetCPUInformation()->m_nLogicalProcessors};

  int test_num{1};
  for (int interleave{0}; interleave <= 1; interleave++) {
    for (unsigned sleep_ms{0}; sleep_ms <= 5; sleep_ms += 5) {
      Msg("RunThreadPoolTests:     Starting test #%d. Sleep %u ms, "
          "interleave '%s', max threads %d.\n",
          test_num, sleep_ms, interleave ? "true" : "false", max_threads_num);

      for (int i{1}; i <= max_threads_num; i += 2) {
        CountJob::m_nCount.store(0, std::memory_order_relaxed);

        ThreadPoolStartParams_t params;
        params.nThreads = i;
        params.fDistribute = should_distribute ? TRS_TRUE : TRS_FALSE;
        if (!pool->Start(params, "CountTstJob")) {
          TP_TEST_EXPECT(false, "Test #%d. Unable to start pool with %d threads.\n",
                         test_num, i);
          continue;
        }

        // Pool may start fewer threads than requested (capped / failed).
        const intp threads_num{pool->NumThreads()};

        CThreadEvent done_event;
        CFastTimer timer, suspendTimer;

        if (!interleave) pool->SuspendExecution();

        std::unique_ptr<CountJob> jobs[kJobsNum];
        {
          RunCodeAtScopeExitOpt( !interleave, pool->ResumeExecution() );

          suspendTimer.Start();
          timer.Start();

          for (size_t j{0}; j < std::size(jobs); j++) {
            jobs[j] = std::make_unique<CountJob>(
                done_event, sleep_ms, static_cast<int>(ssize(jobs)), do_work);
            jobs[j]->SetFlags(JF_QUEUE);

            pool->AddJob(jobs[j].get());

            if (should_sleep && j % 16 == 0) ThreadSleep(0);
          }
        }

        const bool waited_all{should_finish && sleep_ms <= 1};
        if (waited_all) {
          done_event.Wait();
        }

        const int total_jobs_at_finish_num{
            CountJob::m_nCount.load(std::memory_order_relaxed)};
        timer.End();

        {
          pool->SuspendExecution();
          RunCodeAtScopeExit(pool->ResumeExecution());

          suspendTimer.End();
        }

        // Stop() finishes running jobs and aborts queued ones, so after it
        // every job must be finished and the pool holds no references.
        pool->Stop();
        done_event.Reset();

        std::unique_ptr<int[]> jobs_per_thread_num{
            std::make_unique<int[]>(threads_num)};
        int executed_num{0}, aborted_num{0}, by_workers_num{0};
        for (auto &job : jobs) {
          const int executions{job->GetExecutionsNum()};
          TP_TEST_EXPECT(executions <= 1,
                         "Test #%d. Job executed %d times.\n", test_num,
                         executions);
          TP_TEST_EXPECT(job->IsFinished(),
                         "Test #%d. Job not finished after Stop(), status %d.\n",
                         test_num, job->GetStatus());

          if (job->GetStatus() == JOB_OK) {
            ++executed_num;
            TP_TEST_EXPECT(executions == 1,
                           "Test #%d. Job reported done but ran %d times.\n",
                           test_num, executions);
          } else if (job->GetStatus() == JOB_STATUS_ABORTED) {
            ++aborted_num;
            TP_TEST_EXPECT(executions == 0,
                           "Test #%d. Aborted job ran %d times.\n", test_num,
                           executions);
          }

          const int thread_idx{job->GetServiceThread()};
          if (thread_idx != -1) {
            TP_TEST_EXPECT(thread_idx >= 0 && thread_idx < threads_num,
                           "Test #%d. Bad service thread %d.\n", test_num,
                           thread_idx);
            if (thread_idx >= 0 && thread_idx < threads_num) {
              ++jobs_per_thread_num[thread_idx];
              ++by_workers_num;
            }

            job->ClearServiceThread();
          }
        }

        const int total_jobs_executed_num{
            CountJob::m_nCount.load(std::memory_order_relaxed)};

        TP_TEST_EXPECT(executed_num + aborted_num == kJobsNum,
                       "Test #%d. %d executed + %d aborted != %d jobs.\n",
                       test_num, executed_num, aborted_num, kJobsNum);
        TP_TEST_EXPECT(total_jobs_executed_num == executed_num,
                       "Test #%d. Counter %d != %d executed jobs.\n", test_num,
                       total_jobs_executed_num, executed_num);
        // No one but workers executes JF_QUEUE jobs here (nobody yields).
        TP_TEST_EXPECT(by_workers_num == executed_num,
                       "Test #%d. %d jobs executed by workers, %d total.\n",
                       test_num, by_workers_num, executed_num);
        if (waited_all) {
          TP_TEST_EXPECT(executed_num == kJobsNum,
                         "Test #%d. Waited for all, but %d of %d executed.\n",
                         test_num, executed_num, kJobsNum);
        }
        TP_TEST_EXPECT(pool->GetJobCount() == 0,
                       "Test #%d. %u jobs counted after Stop().\n", test_num,
                       pool->GetJobCount());

        const double timer_ms{timer.GetDuration().GetMillisecondsF()};
        const double suspend_timer_ms{
            suspendTimer.GetDuration().GetMillisecondsF()};

        Msg("RunThreadPoolTests:     Test #%d. %zd threads -- %d (%d) jobs "
            "processed in %.4fms, %.4fms to suspend (%.4f/%.4f).\n",
            test_num, threads_num, total_jobs_at_finish_num,
            total_jobs_executed_num, timer_ms, suspend_timer_ms - timer_ms,
            SafeDiv(timer_ms, total_jobs_executed_num),
            SafeDiv(suspend_timer_ms, total_jobs_at_finish_num));

        Msg("RunThreadPoolTests:     Test #%d. %zd threads -- %d (%d) jobs "
            "distribution per thread:\n",
            test_num, threads_num, total_jobs_at_finish_num,
            total_jobs_executed_num);

        for (intp j{0}; j < threads_num; ++j) {
          Msg("RunThreadPoolTests:     Test #%d. %zd thread - %d jobs.\n",
              test_num, j, jobs_per_thread_num[j]);
        }
      }

      ++test_num;
    }
  }
}

class ExecuteTestJob : public CJob {
 public:
  explicit ExecuteTestJob(std::atomic_bool &has_execute_error)
      : has_execute_error_{has_execute_error} {}

  JobStatus_t DoExecute() override {
    executions_num_.fetch_add(1, std::memory_order_relaxed);

    const double acc{DoSomeWork(SeedFrom(this))};

    // Completion must not be visible while still executing. Also exercises
    // lazy completion event creation racing with other threads.
    if (AccessEvent()->Check() || IsFinished()) {
      bool expected{false};

      if (has_execute_error_.compare_exchange_strong(
              expected, true, std::memory_order_release,
              std::memory_order_relaxed)) {
        Msg("Forced execution test failed!\n");

        DebuggerBreakIfDebugging();
      }
    }

    return acc != 0.0 ? JOB_OK : -1;
  }

  [[nodiscard]] int GetExecutionsNum() const {
    return executions_num_.load(std::memory_order_relaxed);
  }

 private:
  std::atomic_bool &has_execute_error_;
  std::atomic_int executions_num_{0};
};

class ExecuteTestExecuteJob : public CJob {
 public:
  ExecuteTestExecuteJob(std::unique_ptr<ExecuteTestJob> &job,
                        std::atomic_int &ready_jobs_num,
                        std::atomic_bool &is_ready_2_execute)
      : job_{job},
        ready_jobs_num_{ready_jobs_num},
        is_ready_2_execute_{is_ready_2_execute} {}

  JobStatus_t DoExecute() override {
    ready_jobs_num_.fetch_add(1, std::memory_order_acq_rel);

    while (!is_ready_2_execute_.load(std::memory_order_acquire))
      ThreadPause();

    // Exactly 1-in-10 aborts. Not derived from addresses: allocations with a
    // fixed stride could make it always or never abort.
    const bool should_abort{
        abort_counter_.fetch_add(1, std::memory_order_relaxed) %
            10 ==
        0};
    if (!should_abort)
      job_->Execute();
    else
      job_->Abort();

    // Racing waiter: must see completion right away.
    if (!job_->WaitForFinish(0) || !job_->AccessEvent()->Check()) {
      failed_.store(true, std::memory_order_relaxed);
    }

    ready_jobs_num_.fetch_sub(1, std::memory_order_acq_rel);

    return JOB_OK;
  }

  static std::atomic_bool failed_;

 private:
  static std::atomic_uint abort_counter_;

  std::unique_ptr<ExecuteTestJob> &job_;
  std::atomic_int &ready_jobs_num_;
  std::atomic_bool &is_ready_2_execute_;
};

std::atomic_bool ExecuteTestExecuteJob::failed_{false};
std::atomic_uint ExecuteTestExecuteJob::abort_counter_{0};

void TestForcedExecute(IThreadPool *pool) {
  const int max_threads_num{GetCPUInformation()->m_nLogicalProcessors};
  constexpr int tests_num{10};

  Msg("Test forced execution starts (%d tests, %d threads).\n", tests_num,
      max_threads_num);

  for (int tests{0}; tests < tests_num; tests++) {
    for (int i{1}; i <= max_threads_num; i += 2) {
      std::atomic_int ready_jobs_num{0};

      ThreadPoolStartParams_t params;
      params.nThreads = i;
      params.fDistribute = TRS_TRUE;
      if (!pool->Start(params, "FeTestJob")) {
        TP_TEST_EXPECT(false, "Forced execution: unable to start pool with %d threads.\n", i);
        continue;
      }

      // All executors must run at once, so wait for as many as there really
      // are threads. Waiting for requested count spun forever if pool started
      // fewer threads.
      const int threads_num{static_cast<int>(pool->NumThreads())};

      std::atomic_bool has_execute_error{false};
      ExecuteTestExecuteJob::failed_.store(false);
      std::unique_ptr<ExecuteTestJob> jobs[kJobsNum];

      for (auto &job : jobs) {
        job = std::make_unique<ExecuteTestJob>(has_execute_error);

        std::atomic_bool is_ready_2_execute{false};

        for (int k{0}; k < threads_num; k++) {
          auto *pJob = new ExecuteTestExecuteJob{job, ready_jobs_num,
                                                 is_ready_2_execute};
          pJob->SetFlags(JF_QUEUE);

          pool->AddJob(pJob);

          pJob->Release();
        }

        while (ready_jobs_num.load(std::memory_order_acquire) <
               threads_num) {
          ThreadPause();
        }

        is_ready_2_execute.store(true, std::memory_order_release);

        ThreadSleep(0);

        job->Execute();

        // Waiter may also be the one creating completion event.
        TP_TEST_EXPECT(job->WaitForFinish(0),
                       "Forced execution: job not finished after Execute().\n");

        while (ready_jobs_num.load(std::memory_order_acquire) >
               0) {
          ThreadPause();
        }

        // Exactly one of: executed once, or aborted before any execution.
        const int executions{job->GetExecutionsNum()};
        const JobStatus_t status{job->GetStatus()};
        TP_TEST_EXPECT(
            (status == JOB_OK && executions == 1) ||
                (status == JOB_STATUS_ABORTED && executions == 0),
            "Forced execution: status %d with %d executions.\n", status,
            executions);
      }

      pool->Stop();

      TP_TEST_EXPECT(!has_execute_error.load(),
                     "Forced execution: job observed finished while running "
                     "(%d threads).\n",
                     threads_num);
      TP_TEST_EXPECT(!ExecuteTestExecuteJob::failed_.load(),
                     "Forced execution: racing waiter didn't see completion "
                     "(%d threads).\n",
                     threads_num);
    }
  }

  Msg("Test forced execution completed (%d tests, %d threads).\n", tests_num,
      max_threads_num);
}

// Restricts the test to a single core.
//
// Windows threads inherit *process* affinity (and the pool resets workers to
// it when not distributing), so pinning only the calling thread left workers
// on all cores. Restrict the process there. POSIX threads inherit affinity of
// the creating thread, so pinning the calling thread is enough.
class ScopedSingleCore {
 public:
  ScopedSingleCore() {
#ifdef _WIN32
    DWORD_PTR system_mask;
    if (::GetProcessAffinityMask(::GetCurrentProcess(), &process_mask_,
                                 &system_mask) &&
        ::SetProcessAffinityMask(::GetCurrentProcess(), 1)) {
      active_ = true;
    }
#else
    ThreadSetAffinity(ThreadHandle_t{}, 1);
    active_ = true;
#endif
  }

  ~ScopedSingleCore() {
    if (!active_) return;
#ifdef _WIN32
    ::SetProcessAffinityMask(::GetCurrentProcess(), process_mask_);
#else
    ThreadSetAffinity(ThreadHandle_t{}, std::numeric_limits<std::uintptr_t>::max());
#endif
  }

  ScopedSingleCore(const ScopedSingleCore &) = delete;
  ScopedSingleCore &operator=(const ScopedSingleCore &) = delete;

  [[nodiscard]] bool IsActive() const { return active_; }

 private:
#ifdef _WIN32
  DWORD_PTR process_mask_{0};
#endif
  bool active_{false};
};

void TestSingleCore(IThreadPool *pool, bool should_sleep,
                    bool should_finish, bool do_work) {
  const ScopedSingleCore single_core;
  if (!single_core.IsActive()) {
    Warning("RunThreadPoolTests:     Unable to restrict to one core, skipped.\n");
    return;
  }

  Test(pool, false, should_sleep, should_finish, do_work);
}

void RunAllConfigurations(IThreadPool *pool, bool should_finish,
                          bool do_work) {
  Msg("RunThreadPoolTests:     Non-distribute\n");
  Test(pool, false, true, should_finish, do_work);

  Msg("RunThreadPoolTests:     Distribute\n");
  Test(pool, true, true, should_finish, do_work);

  Msg("RunThreadPoolTests:     One core (sleep)\n");
  TestSingleCore(pool, true, should_finish, do_work);

  Msg("RunThreadPoolTests:     NO Sleep\n");
  Test(pool, false, false, should_finish, do_work);

  Msg("RunThreadPoolTests:     Distribute\n");
  Test(pool, true, false, should_finish, do_work);

  Msg("RunThreadPoolTests:     One core (no sleep)\n");
  TestSingleCore(pool, false, should_finish, do_work);
}

}  // namespace

namespace s1::engine::tests::thread_pool {

void RunThreadPoolTests() {
  IThreadPool *pool = CreateThreadPool();
  RunCodeAtScopeExitOpt( pool, DestroyThreadPool( pool ) );

  g_failures_num.store(0, std::memory_order_relaxed);

  Msg("RunThreadPoolTests: Job distribution speed:\n");

  for (int i = 0; i < 2; i++) {
    const bool run_2_completition{i % 2 != 0};
    RunAllConfigurations(pool, run_2_completition, false);
  }

  Msg("RunThreadPoolTests: Jobs doing work:\n");
  for (int i = 0; i < 2; i++) {
    RunAllConfigurations(pool, true, true);
  }

  TestForcedExecute(pool);

  const int failures_num{g_failures_num.load(std::memory_order_relaxed)};
  if (failures_num == 0) {
    Msg("RunThreadPoolTests: PASSED.\n");
  } else {
    Warning("RunThreadPoolTests: FAILED, %d checks failed.\n", failures_num);
  }
}

}  // namespace s1::engine::tests::thread_pool