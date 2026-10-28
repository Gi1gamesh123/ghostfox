#include "web_audio/dsp_math.h"

#include <bit>
#include <cmath>
#include <cstdint>

// Port of fdlibm sinf/asinf (modules/fdlibm/src/s_sinf.cpp, k_sinf.cpp,
// k_cosf.cpp, e_rem_pio2f.cpp, e_asinf.cpp). Gecko reaches sinf through the
// compressor's gain warp and asinf through its envelope, so both must reproduce
// fdlibm's polynomial and reduction order exactly.
namespace ruyijs::web_audio::dsp {
namespace {

int32_t Word(float x) { return std::bit_cast<int32_t>(x); }

constexpr double kS1 = -0x15555554cbac77.0p-55;
constexpr double kS2 = 0x111110896efbb2.0p-59;
constexpr double kS3 = -0x1a00f9e2cae774.0p-65;
constexpr double kS4 = 0x16cd878c3b46a7.0p-71;
constexpr double kC0 = -0x1ffffffd0c5e81.0p-54;
constexpr double kC1 = 0x155553e1053a42.0p-57;
constexpr double kC2 = -0x16c087e80f1e27.0p-62;
constexpr double kC3 = 0x199342e0ee5069.0p-68;

// k_sinf.cpp: |sin(x)/x - s(x)| < 2**-37.5, |x| < pi/4.
float KernelSin(double x) {
  const double z = x * x;
  const double w = z * z;
  const double r = kS3 + z * kS4;
  const double s = z * x;
  return float((x + s * (kS1 + z * kS2)) + s * w * r);
}

// k_cosf.cpp: |cos(x) - c(x)| < 2**-34.1, |x| < pi/4.
float KernelCos(double x) {
  const double z = x * x;
  const double w = z * z;
  const double r = kC2 + z * kC3;
  return float(((1.0 + z * kC0) + w * kC1) + (w * z) * r);
}

constexpr double kInvPio2 = 6.36619772367581382433e-01;
constexpr double kPio2_1 = 1.57079631090164184570e+00;
constexpr double kPio2_1t = 1.58932547735281966916e-08;

// Medium-size argument reduction of e_rem_pio2f.cpp: x rem pi/2 in double.
int32_t RemPio2(float x, double* y) {
  const double fn = (double(x) * kInvPio2 + 0x1.8p52) - 0x1.8p52;
  const int32_t n = int32_t(fn);
  const double r = double(x) - fn * kPio2_1;
  const double w = fn * kPio2_1t;
  *y = r - w;
  return n;
}

// Small multiples of pi/2 rounded to double precision.
constexpr double kS1Pio2 = 1 * 1.570796326794896558e+00;
constexpr double kS2Pio2 = 2 * 1.570796326794896558e+00;
constexpr double kS3Pio2 = 3 * 1.570796326794896558e+00;
constexpr double kS4Pio2 = 4 * 1.570796326794896558e+00;

}  // namespace

// sinf
float Sin(float x) {
  const int32_t hx = Word(x), ix = hx & 0x7fffffff;
  if (ix <= 0x3f490fda) {  // |x| ~<= pi/4
    if (ix < 0x39800000 && int32_t(x) == 0) return x;
    return KernelSin(double(x));
  }
  if (ix <= 0x407b53d1) {    // |x| ~<= 5*pi/4
    if (ix <= 0x4016cbe3) {  // |x| ~<= 3pi/4
      if (hx > 0) return KernelCos(double(x) - kS1Pio2);
      return -KernelCos(double(x) + kS1Pio2);
    }
    return KernelSin((hx > 0 ? kS2Pio2 : -kS2Pio2) - double(x));
  }
  if (ix <= 0x40e231d5) {    // |x| ~<= 9*pi/4
    if (ix <= 0x40afeddf) {  // |x| ~<= 7*pi/4
      if (hx > 0) return -KernelCos(double(x) - kS3Pio2);
      return KernelCos(double(x) + kS3Pio2);
    }
    return KernelSin(double(x) + (hx > 0 ? -kS4Pio2 : kS4Pio2));
  }
  if (ix >= 0x7f800000) return x - x;  // sin(Inf or NaN) is NaN
  double y = 0.0;
  const int32_t n = RemPio2(x, &y);
  switch (n & 3) {
    case 0:
      return KernelSin(y);
    case 1:
      return KernelCos(y);
    case 2:
      return KernelSin(-y);
    default:
      return -KernelCos(y);
  }
}

// cosf
float Cos(float x) {
  const int32_t hx = Word(x), ix = hx & 0x7fffffff;
  if (ix <= 0x3f490fda) {  // |x| ~<= pi/4
    if (ix < 0x39800000 && int32_t(x) == 0) return 1.0f;
    return KernelCos(double(x));
  }
  if (ix <= 0x407b53d1) {           // |x| ~<= 5*pi/4
    if (ix > 0x4016cbe3)            // |x| ~> 3*pi/4
      return -KernelCos(double(x) + (hx > 0 ? -kS2Pio2 : kS2Pio2));
    if (hx > 0) return KernelSin(kS1Pio2 - double(x));
    return KernelSin(double(x) + kS1Pio2);
  }
  if (ix <= 0x40e231d5) {  // |x| ~<= 9*pi/4
    if (ix > 0x40afeddf)   // |x| ~> 7*pi/4
      return KernelCos(double(x) + (hx > 0 ? -kS4Pio2 : kS4Pio2));
    if (hx > 0) return KernelSin(double(x) - kS3Pio2);
    return KernelSin(-kS3Pio2 - double(x));
  }
  if (ix >= 0x7f800000) return x - x;  // cos(Inf or NaN) is NaN
  double y = 0.0;
  const int32_t n = RemPio2(x, &y);
  switch (n & 3) {
    case 0:
      return KernelCos(y);
    case 1:
      return KernelSin(-y);
    case 2:
      return -KernelCos(y);
    default:
      return KernelSin(y);
  }
}

// __ieee754_asinf
float Asin(float x) {
  constexpr float kP0 = 1.6666586697e-01f, kP1 = -4.2743422091e-02f;
  constexpr float kP2 = -8.6563630030e-03f, kQ1 = -7.0662963390e-01f;
  constexpr double kPio2 = 1.570796326794896558e+00;
  const int32_t hx = Word(x), ix = hx & 0x7fffffff;
  if (ix >= 0x3f800000) {  // |x| >= 1
    if (ix == 0x3f800000) return float(x * kPio2);
    return (x - x) / (x - x);
  }
  if (ix < 0x3f000000) {  // |x| < 0.5
    if (ix < 0x39800000) return x;
    const float t = x * x;
    const float p = t * (kP0 + t * (kP1 + t * kP2));
    const float q = 1.0f + t * kQ1;
    const float w = p / q;
    return x + x * w;
  }
  float w = 1.0f - std::fabs(x);
  float t = w * 0.5f;
  const float p = t * (kP0 + t * (kP1 + t * kP2));
  const float q = 1.0f + t * kQ1;
  const double s = std::sqrt(double(t));
  w = p / q;
  t = float(kPio2 - 2.0 * (s + s * double(w)));
  return hx > 0 ? t : -t;
}

}  // namespace ruyijs::web_audio::dsp
