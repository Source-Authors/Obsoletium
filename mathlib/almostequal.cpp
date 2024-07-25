// Copyright Valve Corporation, All rights reserved.
//
// Purpose: Fast ways to compare equality of two floats.  Assumes
// sizeof(float) == sizeof(int) and we are using IEEE format.
//
// Source:
// https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/

#include <cfloat>
#include <cmath>
#include <limits>

#include "mathlib/mathlib.h"

namespace {

/**
 * @brief Convert float to int.
 * @param a Float in IEEE-754 format.
 * @return Int.
 */
[[nodiscard]] inline int FloatAsInt(float a) {
  static_assert(sizeof(float) == sizeof(int));
  static_assert(std::numeric_limits<float>::is_iec559);

  int v;
  std::memcpy( &v, &a, sizeof(v) );
  return v;
}

/**
 * @brief Convert float to 2 complement int.
 * @param a Float.
 * @return 2 complement int.
 */
[[nodiscard]] inline int FloatAs2ComplementInt(float a) {
  int i{FloatAsInt(a)};

  if (i < 0) i = 0x80000000 - i;

  return i;
}

/**
 * @brief Is IEEE-754 float infinite (either -Inf or +Inf)?
 * @param a Float.
 * @return true if -Inf or +Inf, false otherwise.
 */
[[nodiscard]] inline bool IsInfinite(float a) {
  constexpr int kInfAsInt{0x7F800000};

  // An infinity has an exponent of 255 (shift left 23 positions) and
  // a zero mantissa.  There are two infinities - positive and negative.
  return (FloatAsInt(a) & 0x7FFFFFFF) == kInfAsInt;
}

/**
 * @brief Is IEEE-754 float NaN?
 * @param a Float
 * @return true if NaN, false otherwise.
 */
[[nodiscard]] inline bool IsNan(float a) {
  const int i{FloatAsInt(a)};
  // a NAN has an exponent of 255 (shifted left 23 positions) and
  // a non-zero mantissa.
  const int exp{i & 0x7F800000};
  const int mantissa{i & 0x007FFFFF};

  return exp == 0x7F800000 && mantissa != 0;
}

/**
 * @brief Get IEEE-754 float sign.
 * @param a Float.
 * @return Sign, -1 (< 0), 0 (0), +1 (> 0).
 */
[[nodiscard]] inline int BitSign(float a) {
  // The sign bit of a number is the high bit.
  return FloatAsInt(a) & 0x80000000;
}

}  // namespace

/**
 * @brief Compare floats using Units in the Last Place precision.
 * @param a Float 1.
 * @param b Float 2.
 * @param maxUlps Max distance between floats in ULPs to assume floats are
 * equal.
 * @return true if floats are equal according to ULPs, false otherwise.
 */
bool XM_CALLCONV AlmostEqual(float a, float b, int maxUlps) {
  // There are several optional checks that you can do, depending on what
  // behavior you want from your floating point comparisons.  These checks
  // should not be necessary and they are included mainly for completeness.

  // If a or b are infinity (positive or negative) then only return true if they
  // are exactly equal to each other - that is, if they are both infinities of
  // the same sign.  This check is only needed if you will be generating
  // infinities and you don't want them 'close' to numbers near FLT_MAX.
  if (IsInfinite(a) || IsInfinite(b)) return a == b;

  // If a or b are a NAN, return false.  NANs are equal to nothing, not even
  // themselves.  This check is only needed if you will be generating NANs and
  // you use a maxUlps greater than 4 million or you want to ensure that a NAN
  // does not equal itself.
  if (IsNan(a) || IsNan(b)) return false;

  // After adjusting floats so their representations are lexicographically
  // ordered as twos-complement integers a very small positive number will
  // compare as 'close' to a very small negative number.  If this is not
  // desireable, and if you are on a platform that supports subnormals (which is
  // the only place the problem can show up) then you need this check.  The
  // check for a == b is because zero and negative zero have different signs but
  // are equal to each other.
  if (BitSign(a) != BitSign(b)) return a == b;

  // Make ai lexicographically ordered as a twos-complement int.
  const int ai{FloatAs2ComplementInt(a)};
  // Make bi lexicographically ordered as a twos-complement int.
  const int bi{FloatAs2ComplementInt(b)};

  // Now we can compare ai and bi to find out how far apart a and b
  // are.
  const int ulps{abs(ai - bi)};

  return ulps <= maxUlps;
}
