// Copyright Valve Corporation, All rights reserved.
//
//

#include "pacifier.h"

#include "tier0/basetypes.h"
#include "tier0/dbg.h"

namespace {

int g_LastPacifierDrawn = -1;
bool g_bPacifierSuppressed = false;

}  // namespace

void StartPacifier(char const *pPrefix) {
  Msg("%s", pPrefix);

  g_LastPacifierDrawn = -1;

  UpdatePacifier(0.001f);
}

void UpdatePacifier(float flPercent) {
  constexpr int forty = 40;

  const int it = std::clamp(static_cast<int>(flPercent * forty),
                            g_LastPacifierDrawn, forty);

  if (it != g_LastPacifierDrawn && !g_bPacifierSuppressed) {
    for (int i = g_LastPacifierDrawn + 1; i <= it; i++) {
      const auto dv = div(i, 4);

      if (dv.rem == 0) {
        Msg("%d", dv.quot);
      } else {
        if (i != forty) Msg(".");
      }
    }

    g_LastPacifierDrawn = it;
  }
}

void EndPacifier(bool bCarriageReturn) {
  UpdatePacifier(1);

  if (bCarriageReturn && !g_bPacifierSuppressed) Msg("\n");
}

void SuppressPacifier(bool bSuppress) { g_bPacifierSuppressed = bSuppress; }
