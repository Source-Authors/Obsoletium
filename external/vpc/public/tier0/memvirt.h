// Copyright Valve Corporation, All rights reserved.
//
// Purpose: CVirtualMemoryManager interface

#ifndef VPC_TIER0_MEMVIRT_H_
#define VPC_TIER0_MEMVIRT_H_

#define VMM_KB (1024)
#define VMM_MB (1024 * VMM_KB)

#ifdef _PS3
// Total virtual address space reserved by CVirtualMemoryManager on startup:
#define VMM_VIRTUAL_SIZE (512 * VMM_MB)
#define VMM_PAGE_SIZE (64 * VMM_KB)
#endif

// Allocate virtual sections via IMemAlloc::AllocateVirtualMemorySection
abstract_class IVirtualMemorySection {
 public:
  // Information about memory section
  virtual void *GetBaseAddress() = 0;
  virtual size_t GetPageSize() = 0;
  virtual size_t GetTotalSize() = 0;

  // Functions to manage physical memory mapped to virtual memory
  virtual bool CommitPages(void *pvBase, size_t numBytes) = 0;
  virtual void DecommitPages(void *pvBase, size_t numBytes) = 0;

  // Release the physical memory and associated virtual address space
  virtual void Release() = 0;
};

// Get the IVirtualMemorySection associated with a given memory address (if
// any):
extern IVirtualMemorySection *GetMemorySectionForAddress(void *pAddress);

#endif  // VPC_TIER0_MEMVIRT_H_
