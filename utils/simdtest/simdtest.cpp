// Copyright Valve Corporation, All rights reserved.

#include "mathlib/mathlib.h"
#include "mathlib/ssemath.h"
#include "tier2/tier2.h"

#include <iostream>

#include "tier0/memdbgon.h"

// #define RECORD_OUTPUT 1

namespace {

constexpr inline int PROBLEM_SIZE{1000};
constexpr inline int N_ITERS{10000};

bool SIMDTest(FourVectors (&xyz)[PROBLEM_SIZE],
              fltx4 (&creation_time)[PROBLEM_SIZE]) {
  const Vector StartPnt(0, 0, 0);
  const Vector MidP(0, 0, 100);
  const Vector EndPnt(100, 0, 50);

  // This app doesn't go through regular engine init, so init FPU/VPU math
  // behaviour here:
  SetupFPUControlWord();
  TestVPUFlags();

  // Initialize g_XYZ[] and g_CreationTime[]
  SeedRandSIMD(1987301);

  float fourStartTimes[4];
  Vector fourPoints[4];
  Vector offset;

  for (int i = 0; i < PROBLEM_SIZE; i++) {
    for (int j = 0; j < 4; j++) {
      float t = (j + 4 * i) / (4.0f * (PROBLEM_SIZE - 1));

      fourStartTimes[j] = t;
      fourPoints[j] = StartPnt + t * (EndPnt - StartPnt);

      offset.Random(-10.0f, +10.0f);

      fourPoints[j] += offset;
    }

    xyz[i].LoadAndSwizzle(fourPoints[0], fourPoints[1], fourPoints[2],
                          fourPoints[3]);

    const DirectX::XMFLOAT4A t(fourStartTimes);
    creation_time[i] = DirectX::XMLoadFloat4A(&t);
  }

#ifdef RECORD_OUTPUT
  char outputBuffer[1024];
  Q_snprintf(outputBuffer, sizeof(outputBuffer),
             "float testOutput[%d][4][3] = {\n", N_ITERS);
  Warning(outputBuffer);
#endif  // RECORD_OUTPUT

  double STime = Plat_FloatTime();
  bool bChangedSomething = false;
  for (int i = 0; i < N_ITERS; i++) {
    float t = i * (1.0F / N_ITERS);

    FourVectors *__restrict pXYZ = xyz;
    fltx4 *__restrict pCreationTime = creation_time;

    fltx4 CurTime = ReplicateX4(t);
    fltx4 TimeScale = ReplicateX4(1.0F / (max(0.001F, 1.0F)));

    // calculate radius spline
    bool bConstantRadius = true;
    fltx4 Rad0 = ReplicateX4(2.0);
    fltx4 Radm = Rad0;
    fltx4 Rad1 = Rad0;

    fltx4 RadmMinusRad0 = SubSIMD(Radm, Rad0);
    fltx4 Rad1MinusRadm = SubSIMD(Rad1, Radm);

    fltx4 SIMDMinDist = ReplicateX4(2.0);
    fltx4 SIMDMinDist2 = ReplicateX4(2.0 * 2.0);

    fltx4 SIMDMaxDist = MaxSIMD(Rad0, MaxSIMD(Radm, Rad1));
    fltx4 SIMDMaxDist2 = MulSIMD(SIMDMaxDist, SIMDMaxDist);

    FourVectors StartP;
    StartP.DuplicateVector(StartPnt);

    FourVectors MiddleP;
    MiddleP.DuplicateVector(MidP);

    // form delta terms needed for quadratic bezier
    FourVectors Delta0;
    Delta0.DuplicateVector(MidP - StartPnt);

    FourVectors Delta1;
    Delta1.DuplicateVector(EndPnt - MidP);
    int nLoopCtr = PROBLEM_SIZE;
    do {
      fltx4 TScale = MinSIMD(
          Four_Ones, MulSIMD(TimeScale, SubSIMD(CurTime, *pCreationTime)));

      // bezier(a,b,c,t)=lerp( lerp(a,b,t),lerp(b,c,t),t)
      FourVectors L0 = Delta0;
      L0 *= TScale;
      L0 += StartP;

      FourVectors L1 = Delta1;
      L1 *= TScale;
      L1 += MiddleP;

      FourVectors Center = L1;
      Center -= L0;
      Center *= TScale;
      Center += L0;

      FourVectors pts_original = *(pXYZ);
      FourVectors pts = pts_original;
      pts -= Center;

      // calculate radius at the point. !!speed!! - use special case for
      // constant radius

      fltx4 dist_squared = pts * pts;
      fltx4 TooFarMask = CmpGtSIMD(dist_squared, SIMDMaxDist2);
      if ((!bConstantRadius) && (!IsAnyNegative(TooFarMask))) {
        // need to calculate and adjust for true radius =- we've only trivially
        // rejected note voodoo here - we update simdmaxdist for true radius,
        // but not max dist^2, since that's used only for the trivial reject
        // case, which we've already done
        fltx4 R0 = MaddSIMD(RadmMinusRad0, TScale, Rad0);
        fltx4 R1 = MaddSIMD(Rad1MinusRadm, TScale, Radm);
        SIMDMaxDist = MaddSIMD(SubSIMD(R1, R0), TScale, R0);

        // now that we know the true radius, update our mask
        TooFarMask = CmpGtSIMD(dist_squared, MulSIMD(SIMDMaxDist, SIMDMaxDist));
      }

      fltx4 TooCloseMask = CmpLtSIMD(dist_squared, SIMDMinDist2);
      fltx4 NeedAdjust = OrSIMD(TooFarMask, TooCloseMask);
      if (IsAnyNegative(NeedAdjust))  // any out of bounds?
      {
        // change squared distance into approximate rsqr root
        fltx4 guess = ReciprocalSqrtEstSIMD(dist_squared);
        // newton iteration for 1/sqrt(x) : y(n+1)=1/2 (y(n)*(3-x*y(n)^2));
        guess = MulSIMD(
            guess,
            MsubSIMD(dist_squared, MulSIMD(guess, guess),Four_Threes));
        guess = MulSIMD(Four_PointFives, guess);
        pts *= guess;

        FourVectors clamp_far = pts;
        clamp_far *= SIMDMaxDist;
        clamp_far += Center;
        FourVectors clamp_near = pts;
        clamp_near *= SIMDMinDist;
        clamp_near += Center;
        pts.x =
            MaskedAssign(TooCloseMask, clamp_near.x,
                         MaskedAssign(TooFarMask, clamp_far.x, pts_original.x));
        pts.y =
            MaskedAssign(TooCloseMask, clamp_near.y,
                         MaskedAssign(TooFarMask, clamp_far.y, pts_original.y));
        pts.z =
            MaskedAssign(TooCloseMask, clamp_near.z,
                         MaskedAssign(TooFarMask, clamp_far.z, pts_original.z));
        *(pXYZ) = pts;
        bChangedSomething = true;
      }

#ifdef RECORD_OUTPUT
      if (nLoopCtr == 257) {
        Q_snprintf(outputBuffer, sizeof(outputBuffer),
                   "/*%04d:*/ { {%+14e,%+14e,%+14e}, {%+14e,%+14e,%+14e}, "
                   "{%+14e,%+14e,%+14e}, {%+14e,%+14e,%+14e} },\n",
                   i, pXYZ->X(0), pXYZ->Y(0), pXYZ->Z(0), pXYZ->X(1),
                   pXYZ->Y(1), pXYZ->Z(1), pXYZ->X(2), pXYZ->Y(2), pXYZ->Z(2),
                   pXYZ->X(3), pXYZ->Y(3), pXYZ->Z(3));
        Warning(outputBuffer);
      }
#endif  // RECORD_OUTPUT

      ++pXYZ;
      ++pCreationTime;
    } while (--nLoopCtr);
  }
  double ETime = Plat_FloatTime() - STime;

#ifdef RECORD_OUTPUT
  Q_snprintf(outputBuffer, sizeof(outputBuffer), "         };\n");
  Warning(outputBuffer);
#endif  // RECORD_OUTPUT

  std::cout << "elapsed time=" << ETime
            << " p/s=" << (4.0 * PROBLEM_SIZE * N_ITERS) / ETime << "\n";
  return bChangedSomething;
}

fltx4 SSEClassTest(fltx4 val) {
  fltx4 result = Four_Zeros;

  for (int i = 0; i < N_ITERS; i++) {
    result = SubSIMD(val, result);
    result = MulSIMD(val, result);
    result = AddSIMD(val, result);
    result = MinSIMD(val, result);
  }

  FourVectors result4;
  result4.x = result;
  result4.y = result;
  result4.z = result;

  for (int i = 0; i < N_ITERS; i++) {
    result4 *= result4;
    result4 += result4;
    result4 *= result4;
    result4 += result4;
  }

  result = result4 * result4;
  return result;
}

}  // namespace

int main(int argc, char **argv) {
  InitCommandLineProgram(argc, argv);

  std::cout << "Checking SSE APIs, expect 4 x 0.75\n";

  // This function is useful for inspecting compiler output.
  fltx4 result = SSEClassTest(Four_PointFives);

  std::cout << "(" << SubFloat(result, 0) << "," << SubFloat(result, 1) << ","
            << SubFloat(result, 2) << "," << SubFloat(result, 3) << ")\n";

  FourVectors xyzs[PROBLEM_SIZE];
  fltx4 creation_times[PROBLEM_SIZE];

  // Run the perf. test
  SIMDTest(xyzs, creation_times);

  return 0;
}
