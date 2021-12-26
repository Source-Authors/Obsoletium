// Copyright Valve Corporation, All rights reserved.

#ifndef VPC_TIER0_VROF_SN_H_
#define VPC_TIER0_VROF_SN_H_

// enable this to get detailed SN Tuner markers. PS3 specific
#if defined(SN_TARGET_PS3) && !defined(_CERT)
#define VPROF_SN_LEVEL 0

extern "C" void (*g_pfnPushMarker)(const char* pName);
extern "C" void (*g_pfnPopMarker)();

class CVProfSnMarkerScope {
 public:
  CVProfSnMarkerScope(const char* pszName) { g_pfnPushMarker(pszName); }
  ~CVProfSnMarkerScope() { g_pfnPopMarker(); }
};

#else

class CVProfSnMarkerScope {
 public:
  CVProfSnMarkerScope(const char*) {}
};

#endif

#endif  // VPC_TIER0_VROF_SN_H_