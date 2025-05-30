// Copyright Valve Corporation, All rights reserved.
//
// Thread pool self-tests.

#include "tests_ts_collections.h"

#include <atomic>
#include <memory>

#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier1/generichash.h"
#include "vstdlib/jobthread.h"

#ifdef PLATFORM_WINDOWS_PC
#include "winlite.h"
#endif

#include "tier0/memdbgon.h"

namespace {

class CountJob : public CJob {
 public:
  CountJob(CThreadEvent &done_event, unsigned sleep_ms, int total_count,
           bool should_do_work)
      : done_event_{done_event},
        sleep_ms_{sleep_ms},
        total_count_{total_count},
        should_do_work_{should_do_work} {}

  JobStatus_t DoExecute() override {
    m_nCount.fetch_add(1, std::memory_order::memory_order_relaxed);

    ThreadPause();

    ThreadSleep(sleep_ms_);

    if (should_do_work_) {
      byte memory[1024];

      for (auto &b : memory) b = static_cast<byte>(RandomInt(0, UCHAR_MAX));

      double acc{0.0};
      for (size_t i = 0; i < 50; i++) {
        acc += sqrt(HashBlock(memory, std::size(memory)) +
                    HashBlock(memory, std::size(memory)) + 10.0);
      }

      should_do_work_ = acc != 0.0F;
    }

    if (m_nCount.load(std::memory_order::memory_order_relaxed) ==
        total_count_) {
      done_event_.Set();
    }

    return 0;
  }

  static std::atomic_int m_nCount;

 private:
  CThreadEvent &done_event_;
  const unsigned sleep_ms_;
  const int total_count_;
  bool should_do_work_;
};

std::atomic_int CountJob::m_nCount{0};

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
        CountJob::m_nCount.store(0, std::memory_order::memory_order_relaxed);

        ThreadPoolStartParams_t params;
        params.nThreads = i;
        params.fDistribute = should_distribute ? TRS_TRUE : TRS_FALSE;
        pool->Start(params, "CountTstJob");

        if (!interleave) pool->SuspendExecution();

        CThreadEvent done_event;

        CFastTimer timer, suspendTimer;
        suspendTimer.Start();
        timer.Start();

        std::unique_ptr<CountJob> jobs[4000];
        for (size_t j{0}; j < std::size(jobs); j++) {
          jobs[j] = std::make_unique<CountJob>(
              done_event, sleep_ms, static_cast<int>(ssize(jobs)), do_work);
          jobs[j]->SetFlags(JF_QUEUE);

          pool->AddJob(jobs[j].get());

          if (should_sleep && j % 16 == 0) ThreadSleep(0);
        }

        if (!interleave) pool->ResumeExecution();

        if (should_finish && sleep_ms <= 1) {
          done_event.Wait();
        }

        const int total_jobs_at_finish_num{
            CountJob::m_nCount.load(std::memory_order::memory_order_relaxed)};
        timer.End();

        pool->SuspendExecution();
        suspendTimer.End();
        pool->ResumeExecution();

        pool->Stop();
        done_event.Reset();

        std::unique_ptr<int[]> jobs_per_thread_num{std::make_unique<int[]>(i)};
        for (auto &job : jobs) {
          int thread_idx{job->GetServiceThread()};

          if (thread_idx != -1) {
            ++jobs_per_thread_num[thread_idx];

            job->ClearServiceThread();
          }
        }

        const int total_jobs_executed_num{
            CountJob::m_nCount.load(std::memory_order::memory_order_relaxed)};

        const double timer_ms{timer.GetDuration().GetMillisecondsF()};
        const double suspend_timer_ms{
            suspendTimer.GetDuration().GetMillisecondsF()};

        Msg("RunThreadPoolTests:     Test #%d. %d threads -- %d (%d) jobs "
            "processed in %.4fms, %.4fms to suspend (%.4f/%.4f).\n",
            test_num, i, total_jobs_at_finish_num, total_jobs_executed_num,
            timer_ms, suspend_timer_ms - timer_ms,
            timer_ms / total_jobs_executed_num,
            suspend_timer_ms / total_jobs_at_finish_num);

        Msg("RunThreadPoolTests:     Test #%d. %d threads -- %d (%d) jobs "
            "distribution per thread:\n",
            test_num, i, total_jobs_at_finish_num, total_jobs_executed_num);

        for (int j{0}; j < i; ++j) {
          Msg("RunThreadPoolTests:     Test #%d. %d thread - %d jobs.\n",
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
    byte memory[1024];

    for (auto &b : memory) b = static_cast<byte>(RandomInt(0, UCHAR_MAX));

    double acc{0.0};
    for (unsigned i = 0; i < 50; i++) {
      acc += sqrt(HashBlock(memory, std::size(memory)) +
                  HashBlock(memory, std::size(memory)) + 10.0);
    }

    if (AccessEvent()->Check() || IsFinished()) {
      bool expected{false};

      if (has_execute_error_.compare_exchange_strong(
              expected, true, std::memory_order::memory_order_release,
              std::memory_order::memory_order_relaxed)) {
        Msg("Forced execution test failed!\n");

        DebuggerBreakIfDebugging();
      }
    }

    return acc != 0.0 ? JOB_OK : -1;
  }

 private:
  std::atomic_bool &has_execute_error_;
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
    ready_jobs_num_.fetch_add(1, std::memory_order::memory_order_acquire);

    while (!is_ready_2_execute_) ThreadPause();

    const bool should_abort{RandomInt(1, 10) == 1};
    if (!should_abort)
      job_->Execute();
    else
      job_->Abort();

    ready_jobs_num_.fetch_sub(1, std::memory_order::memory_order_release);

    return JOB_OK;
  }

 private:
  std::unique_ptr<ExecuteTestJob> &job_;
  std::atomic_int &ready_jobs_num_;
  std::atomic_bool &is_ready_2_execute_;
};

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
      pool->Start(params, "FeTestJob");

      std::atomic_bool has_execute_error{false};
      std::unique_ptr<ExecuteTestJob> jobs[4000];

      for (auto &job : jobs) {
        job = std::make_unique<ExecuteTestJob>(has_execute_error);

        std::atomic_bool is_ready_2_execute{false};

        for (int k{0}; k < i; k++) {
          auto *pJob = new ExecuteTestExecuteJob{job, ready_jobs_num,
                                                 is_ready_2_execute};
          pJob->SetFlags(JF_QUEUE);

          pool->AddJob(pJob);

          pJob->Release();
        }

        while (ready_jobs_num.load(std::memory_order::memory_order_relaxed) <
               i) {
          ThreadPause();
        }

        is_ready_2_execute.store(true, std::memory_order::memory_order_relaxed);

        ThreadSleep();

        job->Execute();

        while (ready_jobs_num.load(std::memory_order::memory_order_relaxed) >
               0) {
          ThreadPause();
        }
      }

      pool->Stop();
    }
  }

  Msg("Test forced execution completed (%d tests, %d threads).\n", tests_num,
      max_threads_num);
}

class ScopedThreadPool {
 public:
  ScopedThreadPool() : pool_{CreateThreadPool()} {}
  ~ScopedThreadPool() { DestroyThreadPool(pool_); }

  IThreadPool *operator&() { return pool_; }

 private:
  IThreadPool *pool_;
};

class ScopedThreadAffinity {
 public:
  ScopedThreadAffinity(std::uintptr_t process_affinity, ThreadHandle_t thread,
                       std::uintptr_t new_thread_affinity)
      : thread_{thread}, process_affinity_{process_affinity} {
    ThreadSetAffinity(thread, new_thread_affinity);
  }
  ~ScopedThreadAffinity() { ThreadSetAffinity(thread_, process_affinity_); }

 private:
  ThreadHandle_t thread_;
  const std::uintptr_t process_affinity_;
};

std::uintptr_t GetProcessAffinity() {
#ifdef _WIN32
  DWORD_PTR process_mask = std::numeric_limits<DWORD_PTR>::max();
  DWORD_PTR system_mask = std::numeric_limits<DWORD_PTR>::max();

  ::GetProcessAffinityMask(::GetCurrentProcess(), &process_mask, &system_mask);

  return process_mask;
#else
  return std::numeric_limits<std::uintptr_t>::max();
#endif
}

}  // namespace

namespace se::engine::tests::thread_pool {

void RunThreadPoolTests() {
  ScopedThreadPool pool;

  const std::uintptr_t process_mask{GetProcessAffinity()};

  Msg("RunThreadPoolTests: Job distribution speed:\n");

  for (int i = 0; i < 2; i++) {
    const bool run_2_completition{i % 2 != 0};

    Msg("RunThreadPoolTests:     Non-distribute\n");
    Test(&pool, false, true, run_2_completition);

    Msg("RunThreadPoolTests:     Distribute\n");
    Test(&pool, true, true, run_2_completition);

    {
      Msg("RunThreadPoolTests:     One core (sleep)\n");

      const ScopedThreadAffinity affinity{process_mask, 0, 1};
      Test(&pool, false, true, run_2_completition);
    }

    Msg("RunThreadPoolTests:     NO Sleep\n");
    Test(&pool, false, false, run_2_completition);

    Msg("RunThreadPoolTests:     Distribute\n");
    Test(&pool, true, false, run_2_completition);

    {
      Msg("RunThreadPoolTests:     One core (no sleep)\n");

      const ScopedThreadAffinity affinity{process_mask, 0, 1};
      Test(&pool, false, false, run_2_completition);
    }
  }

  Msg("RunThreadPoolTests: Jobs doing work:\n");
  for (int i = 0; i < 2; i++) {
    const bool run_2_completition = true;

    Msg("RunThreadPoolTests:     Non-distribute\n");
    Test(&pool, false, true, run_2_completition, true);

    Msg("RunThreadPoolTests:     Distribute\n");
    Test(&pool, true, true, run_2_completition, true);

    {
      Msg("RunThreadPoolTests:     One core (sleep)\n");

      const ScopedThreadAffinity affinity{process_mask, 0, 1};
      Test(&pool, false, true, run_2_completition, true);
    }

    Msg("RunThreadPoolTests:     NO Sleep\n");
    Test(&pool, false, false, run_2_completition, true);

    Msg("RunThreadPoolTests:     Distribute\n");
    Test(&pool, true, false, run_2_completition, true);

    {
      Msg("RunThreadPoolTests:     One core (no sleep)\n");

      const ScopedThreadAffinity affinity{process_mask, 0, 1};
      Test(&pool, false, false, run_2_completition, true);
    }
  }

  TestForcedExecute(&pool);
}

}  // namespace se::engine::tests::thread_pool
