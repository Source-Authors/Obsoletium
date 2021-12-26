// Copyright Valve Corporation, All rights reserved.

#ifndef VPC_P4SLN_H_
#define VPC_P4SLN_H_

void GenerateSolutionForPerforceChangelist(
    CProjectDependencyGraph &dependencyGraph, CUtlVector<int> &changelists,
    IBaseSolutionGenerator *pGenerator, const char *pSolutionFilename);

#endif  // VPC_P4SLN_H_
