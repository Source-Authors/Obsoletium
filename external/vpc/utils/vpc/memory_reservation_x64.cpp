// Copyright Valve Corporation, All rights reserved.
//
// Purpose: VPC

#include "memory_reservation_x64.h"

#include <cstdio>
#include "winlite.h"

#include "tier0/memdbgon.h"

namespace vpc::win {

#if defined(_DEBUG) && defined(_WIN64)
// Copyright 2012 Bruce Dawson. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//
// See the License for the specific language governing permissions and
// limitations under the License.
bool ReserveBottomMemoryFor64Bit() noexcept {
  static bool is_initialized{false};
  if (is_initialized) return true;

  // Start by reserving large blocks of address space, and then gradually reduce
  // the size in order to capture all of the fragments.  Technically we should
  // continue down to 64 KiB but stopping at 1 MiB is sufficient to keep most
  // allocators out.

  constexpr size_t kLowMemoryLine{0x100000000ULL};
  constexpr size_t kOneMiB{static_cast<size_t>(1024U) * 1024U};

  size_t total_reserved_bytes{0}, virtual_allocs_num{0}, heap_allocs_num{0};

  // Virtually allocate memory.
  for (size_t size{256U * kOneMiB}; size >= kOneMiB; size /= 2U) {
    for (;;) {
      void *p{::VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS)};
      if (!p) break;

      if (reinterpret_cast<size_t>(p) >= kLowMemoryLine) {
        // We don't need this memory, so release it completely.
        ::VirtualFree(p, 0, MEM_RELEASE);
        break;
      }

      total_reserved_bytes += size;
      ++virtual_allocs_num;
    }
  }

  // Now repeat the same process but making heap allocations, to use up the
  // already reserved heap blocks that are below the 4 GB line.
  HANDLE heap{::GetProcessHeap()};
  for (size_t block_size{static_cast<size_t>(64U) * 1024}; block_size >= 16U;
       block_size /= 2U) {
    for (;;) {
      void *p{::HeapAlloc(heap, 0, block_size)};
      if (!p) break;

      if (reinterpret_cast<size_t>(p) >= kLowMemoryLine) {
        // We don't need this memory, so release it completely.
        ::HeapFree(heap, 0, p);
        break;
      }

      total_reserved_bytes += block_size;
      ++heap_allocs_num;
    }
  }

  // Perversely enough the CRT doesn't use the process heap.  Suck up the memory
  // the CRT heap has already reserved.
  for (size_t block_size{static_cast<size_t>(64U) * 1024U}; block_size >= 16U;
       block_size /= 2U) {
    for (;;) {
      void *p{malloc(block_size)};
      if (!p) break;

      if (reinterpret_cast<size_t>(p) >= kLowMemoryLine) {
        // We don't need this memory, so release it completely.
        free(p);
        break;
      }

      total_reserved_bytes += block_size;
      ++heap_allocs_num;
    }
  }

  // Print diagnostics showing how many allocations we had to make in
  // order to reserve all of low memory, typically less than 200.
  char buffer[1000];
  sprintf_s(buffer,
            "Reserved %1.3f MiB (%zu virtual allocs, %zu heap allocs) of "
            "low-memory.\n",
            total_reserved_bytes / (1024.0 * 1024.0), virtual_allocs_num,
            heap_allocs_num);
  ::OutputDebugStringA(buffer);

  if (total_reserved_bytes > 0) {
    is_initialized = true;
    return true;
  }

  return false;
}
#else
bool ReserveBottomMemoryFor64Bit() noexcept {
  OutputDebugStringA("No memory reserved as not x64 or Release mode.\n");

  return true;
}
#endif

}  // namespace vpc::win