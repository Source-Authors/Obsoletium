// Copyright Valve Corporation, All rights reserved.

#ifndef SRC_LAUNCHER_SCOPED_HEAP_LEAK_DUMPER_H
#define SRC_LAUNCHER_SCOPED_HEAP_LEAK_DUMPER_H

#include "tier0/memalloc.h"
#include "tier0/dbg.h"

namespace src::launcher {

// Dump heap leaks.
class ScopedHeapLeakDumper {
 public:
  ScopedHeapLeakDumper(bool should_check_leaks) noexcept
      : should_check_leaks_(should_check_leaks) {}

  ScopedHeapLeakDumper(const ScopedHeapLeakDumper &) = delete;
  ScopedHeapLeakDumper(ScopedHeapLeakDumper &&) = delete;
  ScopedHeapLeakDumper &operator=(const ScopedHeapLeakDumper &) = delete;
  ScopedHeapLeakDumper &operator=(ScopedHeapLeakDumper &&) = delete;

  ~ScopedHeapLeakDumper() noexcept {
    if (should_check_leaks_) MemAlloc_DumpStats();
  }

 private:
  const bool should_check_leaks_;
};

}  // namespace src::launcher

#endif  // SRC_LAUNCHER_SCOPED_HEAP_LEAK_DUMPER_H
