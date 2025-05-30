// Copyright Valve Corporation, All rights reserved.
//
// Thread-safe aka lock-free collections self-tests.

#include "tests_ts_collections.h"

#include "tier0/tslist.h"

#include <deque>

namespace {

int NUM_TEST = 10000;
int NUM_THREADS;
int NUM_PROCESSORS = 1;

std::atomic_int g_nTested;
std::atomic_int g_nThreads;
std::atomic_int g_nPushThreads;
std::atomic_int g_nPopThreads;
std::atomic_int g_nPushes;
std::atomic_int g_nPops;

std::atomic_bool g_bStart;
std::deque<ThreadHandle_t> g_ThreadHandles;

std::atomic_int *g_pTestBuckets;

template <typename T>
struct ITestContainer {
  virtual ~ITestContainer() {}

  virtual const char *GetType() const = 0;

  virtual void Push(T item) = 0;
  virtual bool Pop(T *pResult) = 0;
  virtual void Purge() = 0;
  virtual bool Validate() { return true; }
  virtual bool IsEmpty() const = 0;
};

template <typename T, bool test_optimizer>
class TSQueueContainer final : public ITestContainer<T> {
 public:
  TSQueueContainer() = default;
  virtual ~TSQueueContainer() = default;

  const char *GetType() const override { return "CTSQueue"; }

  void Push(T item) override {
    queue_.PushItem(item);
    g_nPushes.fetch_add(1, std::memory_order::memory_order_relaxed);
  }

  bool Pop(T *pResult) override {
    if (queue_.PopItem(pResult)) {
      g_nPops.fetch_add(1, std::memory_order::memory_order_relaxed);
      return true;
    }
    return false;
  }

  void Purge() { queue_.Purge(); }

  bool Validate() override { return queue_.ValidateQueue(); }

  bool IsEmpty() const override { return queue_.Count() == 0; }

 private:
  CTSQueue<T, test_optimizer> queue_;
};

template <typename T>
class TSListContainer final : public ITestContainer<T> {
 public:
  TSListContainer() = default;
  virtual ~TSListContainer() = default;

  const char *GetType() const override { return "CTSList"; }

  void Push(T item) override {
    list_.PushItem(item);
    g_nPushes.fetch_add(1, std::memory_order::memory_order_relaxed);
  }

  bool Pop(T *pResult) override {
    if (list_.PopItem(pResult)) {
      g_nPops.fetch_add(1, std::memory_order::memory_order_relaxed);
      return true;
    }

    return false;
  }

  void Purge() { list_.Purge(); }

  bool Validate() override { return true; }

  bool IsEmpty() const override { return list_.Count() == 0; }

 private:
  CTSList<T> list_;
};

void ClearBuckets() {
  for (intp i{0}; i < NUM_TEST; ++i) {
    g_pTestBuckets[i].store(0, std::memory_order::memory_order_relaxed);
  }
}

void IncBucket(int i) {
  // tests can slop over a bit
  if (i < NUM_TEST) {
    g_pTestBuckets[i].fetch_add(1, std::memory_order::memory_order_acquire);
  }
}

void DecBucket(int i) {
  // tests can slop over a bit
  if (i < NUM_TEST) {
    g_pTestBuckets[i].fetch_sub(1, std::memory_order::memory_order_release);
  }
}

void ValidateBuckets() {
  for (int i = 0; i < NUM_TEST; i++) {
    const int bucket =
        g_pTestBuckets[i].load(std::memory_order::memory_order_relaxed);

    if (bucket != 0) {
      Msg("Test bucket %d has an invalid value %d.\n", i, bucket);

      DebuggerBreakIfDebugging();
      return;
    }
  }
}

template <typename T>
unsigned PopThreadFunc(void *ctx) {
  // dimhotepus: Add thread name to aid debugging.
  ThreadSetDebugName("PopTestWork");

  auto *container = static_cast<ITestContainer<T> *>(ctx);

  g_nPopThreads.fetch_add(1, std::memory_order::memory_order_acquire);
  g_nThreads.fetch_add(1, std::memory_order::memory_order_acquire);

  while (!g_bStart.load(std::memory_order::memory_order_relaxed)) {
    ThreadSleep(0);
  }

  int ignored;

  for (;;) {
    if (!container->Pop(&ignored)) {
      if (g_nPushThreads.load(std::memory_order::memory_order_relaxed) == 0) {
        // Pop the rest
        while (container->Pop(&ignored)) {
          ThreadSleep(0);
        }

        break;
      }
    }
  }

  g_nThreads.fetch_sub(1, std::memory_order::memory_order_release);
  g_nPopThreads.fetch_sub(1, std::memory_order::memory_order_release);

  return 0;
}

template <typename T>
unsigned PushThreadFunc(void *ctx) {
  // dimhotepus: Add thread name to aid debugging.
  ThreadSetDebugName("PushTestWork");

  auto *container = static_cast<ITestContainer<T> *>(ctx);

  g_nPushThreads.fetch_add(1, std::memory_order::memory_order_acquire);
  g_nThreads.fetch_add(1, std::memory_order::memory_order_acquire);

  while (!g_bStart.load(std::memory_order::memory_order_relaxed))
    ThreadSleep(0);

  int v;
  while ((v = g_nTested.load(std::memory_order::memory_order_acquire)) <
         NUM_TEST) {
    container->Push(v);

    g_nTested.fetch_add(1, std::memory_order::memory_order_release);
  }

  g_nThreads.fetch_sub(1, std::memory_order::memory_order_release);
  g_nPushThreads.fetch_sub(1, std::memory_order::memory_order_release);

  return 0;
}

void TestStart() {
  g_nTested.store(0, std::memory_order::memory_order_relaxed);
  g_nThreads.store(0, std::memory_order::memory_order_relaxed);
  g_nPushThreads.store(0, std::memory_order::memory_order_relaxed);
  g_nPopThreads.store(0, std::memory_order::memory_order_relaxed);
  g_nPops.store(0, std::memory_order::memory_order_relaxed);
  g_nPushes.store(0, std::memory_order::memory_order_relaxed);
  g_bStart.store(false, std::memory_order::memory_order_release);

  ClearBuckets();
}

void TestWait() {
  while (g_nThreads.load(std::memory_order::memory_order_seq_cst) <
         NUM_THREADS) {
    ThreadSleep(0);
  }

  g_bStart.store(true, std::memory_order::memory_order_release);

  while (g_nThreads.load(std::memory_order::memory_order_seq_cst) > 0) {
    ThreadSleep(50);
  }
}

template <typename T>
void TestEnd(ITestContainer<T> &container, bool bExpectEmpty = true) {
  ValidateBuckets();

  if (g_nPops.load(std::memory_order_seq_cst) !=
      g_nPushes.load(std::memory_order_seq_cst)) {
    Msg("FAIL: Not all items popped.\n");
    return;
  }

  if (container.Validate()) {
    if (!bExpectEmpty || container.IsEmpty()) {
      Msg("pass.\n");
    } else {
      Msg("FAIL: !IsEmpty().\n");
    }
  } else {
    Msg("FAIL: !Validate().\n");
  }

  while (g_ThreadHandles.size()) {
    ThreadHandle_t &h = g_ThreadHandles.front();

    ThreadJoin(h, 0);
    ReleaseThreadHandle(h);

    g_ThreadHandles.pop_front();
  }
}

// Shared Tests for CTSQueue and CTSList
template <typename T>
void PushPopTest(ITestContainer<T> &container) {
  Msg("%s test: single thread push/pop, in order...\n", container.GetType());
  ClearBuckets();

  g_nTested.store(0, std::memory_order::memory_order_relaxed);
  int value;

  while ((value = g_nTested.fetch_add(
              1, std::memory_order::memory_order_acquire)) < NUM_TEST) {
    container.Push(value);

    IncBucket(value);
  }

  container.Validate();

  while (container.Pop(&value)) {
    DecBucket(value);
  }

  TestEnd(container);
}

template <typename T>
void PushPopInterleavedTestGuts(ITestContainer<T> &container) {
  int value;

  for (;;) {
    const bool do_push{rand() % 2 == 0};

    if (do_push &&
        (value = g_nTested.fetch_add(
             1, std::memory_order::memory_order_acq_rel)) < NUM_TEST) {
      container.Push(value);
      IncBucket(value);
    } else if (container.Pop(&value)) {
      DecBucket(value);
    } else {
      if (do_push ||
          g_nTested.load(std::memory_order::memory_order_seq_cst) >= NUM_TEST) {
        break;
      }
    }
  }
}

template <typename T>
void PushPopInterleavedTest(ITestContainer<T> &container) {
  Msg("%s test: single thread push/pop, interleaved...\n", container.GetType());

  // dimhotepus: ms -> mcs to not overflow in 49.7 days.
  srand(static_cast<unsigned>(Plat_USTime() % std::numeric_limits<unsigned>::max()));

  g_nTested.store(0, std::memory_order::memory_order_relaxed);

  ClearBuckets();
  PushPopInterleavedTestGuts(container);
  TestEnd(container);
}

template <typename T>
unsigned PushPopInterleavedTestThreadFunc(void *ctx) {
  // dimhotepus: Add thread name to aid debugging.
  ThreadSetDebugName("PushPopTestWork");

  auto *container = static_cast<ITestContainer<T> *>(ctx);

  g_nThreads.fetch_add(1, std::memory_order::memory_order_acquire);

  while (!g_bStart.load(std::memory_order::memory_order_relaxed))
    ThreadSleep(0);

  PushPopInterleavedTestGuts<T>(*container);

  g_nThreads.fetch_sub(1, std::memory_order::memory_order_release);

  return 0;
}

template <typename T>
void STPushMTPop(ITestContainer<T> &container, bool bDistribute) {
  Msg("%s test: single thread push, multithread pop, %s\n", container.GetType(),
      bDistribute ? "distributed..." : "no affinity...");

  TestStart();
  g_ThreadHandles.push_back(CreateSimpleThread(&PushThreadFunc<T>, &container));

  for (int i = 0; i < NUM_THREADS - 1; i++) {
    ThreadHandle_t hThread = CreateSimpleThread(&PopThreadFunc<T>, &container);
    g_ThreadHandles.push_back(hThread);

    if (bDistribute) {
      int32 mask = 1 << (i % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }

  TestWait();
  TestEnd(container);
}

template <typename T>
void MTPushSTPop(ITestContainer<T> &container, bool bDistribute) {
  Msg("%s test: multithread push, single thread pop, %s\n", container.GetType(),
      bDistribute ? "distributed..." : "no affinity...");

  TestStart();
  g_ThreadHandles.push_back(CreateSimpleThread(&PopThreadFunc<T>, &container));

  for (int i = 0; i < NUM_THREADS - 1; i++) {
    ThreadHandle_t hThread = CreateSimpleThread(&PushThreadFunc<T>, &container);
    g_ThreadHandles.push_back(hThread);

    if (bDistribute) {
      int32 mask = 1 << (i % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }

  TestWait();
  TestEnd(container);
}

template <typename T>
void MTPushMTPop(ITestContainer<T> &container, bool bDistribute) {
  Msg("%s test: multithread push, multithread pop, %s\n", container.GetType(),
      bDistribute ? "distributed..." : "no affinity...");

  TestStart();
  int ct = 0;

  for (int i = 0; i < NUM_THREADS / 2; i++) {
    ThreadHandle_t hThread = CreateSimpleThread(&PopThreadFunc<T>, &container);
    g_ThreadHandles.push_back(hThread);

    if (bDistribute) {
      int32 mask = 1 << (ct++ % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }

  for (int i = 0; i < NUM_THREADS / 2; i++) {
    ThreadHandle_t hThread = CreateSimpleThread(&PushThreadFunc<T>, &container);
    g_ThreadHandles.push_back(hThread);

    if (bDistribute) {
      int32 mask = 1 << (ct++ % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }

  TestWait();
  TestEnd(container);
}

template <typename T>
void MTPushPopPopInterleaved(ITestContainer<T> &container, bool bDistribute) {
  Msg("%s test: multithread interleaved push/pop, %s\n", container.GetType(),
      bDistribute ? "distributed..." : "no affinity...");

  // dimhotepus: ms -> mcs to not overflow in 49.7 days.
  srand(static_cast<unsigned>(Plat_USTime() % std::numeric_limits<unsigned>::max()));
  TestStart();

  for (int i = 0; i < NUM_THREADS; i++) {
    ThreadHandle_t hThread =
        CreateSimpleThread(&PushPopInterleavedTestThreadFunc<T>, &container);
    g_ThreadHandles.push_back(hThread);

    if (bDistribute) {
      int32 mask = 1 << (i % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }

  TestWait();
  TestEnd(container);
}

template <typename T>
void MTPushSeqPop(ITestContainer<T> &container, bool bDistribute) {
  Msg("%s test: multithread push, sequential pop, %s\n", container.GetType(),
      bDistribute ? "distributed..." : "no affinity...");
  TestStart();

  for (int i = 0; i < NUM_THREADS; i++) {
    ThreadHandle_t hThread = CreateSimpleThread(&PushThreadFunc<T>, &container);
    g_ThreadHandles.push_back(hThread);

    if (bDistribute) {
      int32 mask = 1 << (i % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }

  TestWait();
  container.Validate();

  int ignored;
  while (container.Pop(&ignored)) {
  }

  TestEnd(container);
}

template <typename T>
void SeqPushMTPop(ITestContainer<T> &container, bool bDistribute) {
  Msg("%s test: sequential push, multithread pop, %s\n", container.GetType(),
      bDistribute ? "distributed..." : "no affinity...");
  TestStart();

  int v;
  while ((v = g_nTested.fetch_add(1, std::memory_order::memory_order_acquire)) <
         NUM_TEST) {
    container.Push(v + 1);
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    ThreadHandle_t hThread = CreateSimpleThread(&PopThreadFunc<T>, &container);
    g_ThreadHandles.push_back(hThread);

    if (bDistribute) {
      int32 mask = 1 << (i % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }

  TestWait();
  TestEnd(container);
}

template <typename T>
void RunSharedTests(int tests_num, ITestContainer<T> &container) {
  using namespace se::engine::tests::ts_collections;

  NUM_PROCESSORS = GetCPUInformation()->m_nLogicalProcessors;
  int kMaxThreadsNum = NUM_PROCESSORS * 2;

  g_pTestBuckets = new std::atomic_int[NUM_TEST];

  int test_idx{0};
  while (tests_num--) {
    for (NUM_THREADS = 2; NUM_THREADS <= kMaxThreadsNum; NUM_THREADS *= 2) {
      Msg("#%d Testing %s by %d threads:\n", test_idx, container.GetType(),
          NUM_THREADS);

      PushPopTest(container);
      PushPopInterleavedTest(container);
      SeqPushMTPop(container, false);
      STPushMTPop(container, false);
      MTPushSeqPop(container, false);
      MTPushSTPop(container, false);
      MTPushMTPop(container, false);
      MTPushPopPopInterleaved(container, false);

      if (NUM_PROCESSORS > 1) {
        SeqPushMTPop(container, true);
        STPushMTPop(container, true);
        MTPushSeqPop(container, true);
        MTPushSTPop(container, true);
        MTPushMTPop(container, true);
        MTPushPopPopInterleaved(container, true);
      }
    }

    ++test_idx;
  }

  delete[] g_pTestBuckets;
}

}  // namespace

namespace se::engine::tests::ts_collections {

bool RunTSListTests(int list_size, int tests_num) {
  using namespace se::engine::tests::ts_collections;

  NUM_TEST = list_size;

  constexpr int kMaxListSize{
      (1 << (sizeof(TSLHead_t::value.Depth) * CHAR_BIT)) - 1};
  if (NUM_TEST > kMaxListSize) {
    Msg("TSList cannot hold more that %d nodes.\n", kMaxListSize);
    return false;
  }

  TSListContainer<int> list;
  RunSharedTests(tests_num, list);

  Msg("TSList tests done, purging test memory...\n");
  list.Purge();
  Msg("Done.\n");

  return true;
}

bool RunTSQueueTests(int list_size, int tests_num) {
  using namespace se::engine::tests::ts_collections;

  NUM_TEST = list_size;

  TSQueueContainer<int, true> queue;
  RunSharedTests(tests_num, queue);

  Msg("TSQueue tests done, purging test memory...\n");
  queue.Purge();
  Msg("Done.\n");

  return true;
}

}  // namespace se::engine::tests::ts_collections
