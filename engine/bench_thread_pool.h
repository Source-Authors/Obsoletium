// Copyright Valve Corporation, All rights reserved.
//
// Thread pool bencmark.

#ifndef SE_ENGINE_BENCH_THREAD_POOL_H_
#define SE_ENGINE_BENCH_THREAD_POOL_H_

class IThreadPool;

namespace s1::engine::tests::thread_pool
{

void RunThreadPoolBenchmark( IThreadPool *pPool = nullptr );

} // namespace s1::engine::tests::thread_pool

#endif // !SE_ENGINE_BENCH_THREAD_POOL_H_
