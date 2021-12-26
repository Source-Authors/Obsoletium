// Copyright Valve Corporation, All rights reserved.

#ifndef VPC_IBASESOLUTIONGENERATOR_H_
#define VPC_IBASESOLUTIONGENERATOR_H_

#include "dependencies.h"

class IBaseSolutionGenerator {
 public:
  virtual void GenerateSolutionFile(
      const char *pSolutionFilename,
      CUtlVector<CDependency_Project *> &projects) = 0;
};

#endif  // VPC_IBASESOLUTIONGENERATOR_H_
