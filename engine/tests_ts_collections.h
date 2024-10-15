// Copyright Valve Corporation, All rights reserved.
//
// Thread-safe aka lock-free collections self-tests.

#ifndef SE_ENGINE_TESTS_TS_COLLECTIONS_H_
#define SE_ENGINE_TESTS_TS_COLLECTIONS_H_

namespace se::engine::tests::ts_collections {

bool RunTSListTests(int nListSize = 10000, int nTests = 1);
bool RunTSQueueTests(int nListSize = 10000, int nTests = 1);

}  // namespace se::engine::tests::ts_collections

#endif  // !SE_ENGINE_TESTS_TS_COLLECTIONS_H_
