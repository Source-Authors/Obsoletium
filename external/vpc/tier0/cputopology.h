// CpuTopology.h
//
// CpuTopology class declaration.
//
// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef VPC_TIER0_CPU_TOPOLOGY_H_
#define VPC_TIER0_CPU_TOPOLOGY_H_

#include "winlite.h"

class ICpuTopology;

// Name: CpuTopology
// Desc: This class constructs a supported cpu topology implementation object on
//       initialization and forwards calls to it.  This is the Abstraction class
//       in the traditional Bridge Pattern.
class CpuTopology {
 public:
  CpuTopology(BOOL bForceCpuid = FALSE);
  ~CpuTopology();

  BOOL IsDefaultImpl() const;
  DWORD NumberOfProcessCores() const;
  DWORD NumberOfSystemCores() const;
  DWORD_PTR CoreAffinityMask(DWORD coreIdx) const;

  void ForceCpuid(BOOL bForce);

 private:
  void Destroy_();

  ICpuTopology* m_pImpl;
};

#endif  // VPC_TIER0_CPU_TOPOLOGY_H_
