#include "web_audio/dsp_math.h"

#include <bit>
#include <cstdint>

// Port of the fdlibm float exp/log/log10 routines (modules/fdlibm/src
// e_expf.cpp, e_logf.cpp, e_log10f.cpp, k_logf.h). Coefficients and operation
// order are kept verbatim so the results are bit-identical to Gecko's.
namespace ruyijs::web_audio::dsp {
namespace {

int32_t Word(float x) { return std::bit_cast<int32_t>(x); }
float FromWord(int32_t bits) { return std::bit_cast<float>(bits); }
uint32_t UWord(float x) { return std::bit_cast<uint32_t>(x); }
float FromUWord(uint32_t bits) { return std::bit_cast<float>(bits); }

// k_logf.h: |(log(1+s)-log(1-s))/s - Lg(s)| < 2**-34.24.
constexpr float kLg1 = 0xaaaaaa.0p-24f;
constexpr float kLg2 = 0xccce13.0p-25f;
constexpr float kLg3 = 0x91e9ee.0p-25f;
constexpr float kLg4 = 0xf89e26.0p-26f;

float Log1p(float f) {
  const float s = f / (2.0f + f);
  const float z = s * s;
  const float w = z * z;
  const float t1 = w * (kLg2 + w * kLg4);
  const float t2 = z * (kLg1 + w * kLg3);
  const float r = t2 + t1;
  const float hfsq = 0.5f * f * f;
  return s * (hfsq + r);
}

}  // namespace

// __ieee754_expf
float Exp(float x) {
  const uint32_t hxRaw = UWord(x);
  const int32_t xsb = int32_t(hxRaw >> 31) & 1;  // sign bit of x
  uint32_t hx = hxRaw & 0x7fffffff;              // high word of |x|
  if (hx >= 0x42b17218) {                        // if |x|>=88.721...
    if (hx > 0x7f800000) return x + x;           // NaN
    if (hx == 0x7f800000) return xsb == 0 ? x : 0.0f;
    if (x > 8.8721679688e+01f) return 1.0e+30f * 1.0e+30f;
    if (x < -1.0397208405e+02f) return 7.8886090522e-31f * 7.8886090522e-31f;
  }
  float hi = 0.0f, lo = 0.0f;
  int32_t k = 0;
  if (hx > 0x3eb17218) {  // |x| > 0.5 ln2
    if (hx < 0x3f851592) {
      hi = x - (xsb ? -6.9314575195e-01f : 6.9314575195e-01f);
      lo = xsb ? -1.4286067653e-06f : 1.4286067653e-06f;
      k = 1 - xsb - xsb;
    } else {
      k = int32_t(1.4426950216e+00f * x + (xsb ? -0.5f : 0.5f));
      const float t = float(k);
      hi = x - t * 6.9314575195e-01f;
      lo = t * 1.4286067653e-06f;
    }
    x = hi - lo;
  } else if (hx < 0x39000000) {  // |x|<2**-14
    return 1.0f + x;
  }
  const float t = x * x;
  const uint32_t twopk = k >= -125 ? uint32_t(0x7f + k) << 23
                                   : uint32_t(0x7f + (k + 100)) << 23;
  const float c = x - t * (1.6666625440e-1f + t * -2.7667332906e-3f);
  if (k == 0) return 1.0f - ((x * c) / (c - 2.0f) - x);
  const float y = 1.0f - ((lo - (x * c) / (2.0f - c)) - hi);
  if (k >= -125) {
    if (k == 128) return y * 2.0f * 0x1p127f;
    return y * FromUWord(twopk);
  }
  return y * FromUWord(twopk) * 7.8886090522e-31f;
}

// __ieee754_logf
float Log(float x) {
  constexpr float kLn2Hi = 6.9313812256e-01f, kLn2Lo = 9.0580006145e-06f;
  constexpr float kTwo25 = 3.355443200e+07f;
  int32_t ix = Word(x);
  int32_t k = 0;
  if (ix < 0x00800000) {  // x < 2**-126
    if ((ix & 0x7fffffff) == 0) return -kTwo25 / 0.0f;
    if (ix < 0) return (x - x) / 0.0f;
    k -= 25;
    x *= kTwo25;
    ix = Word(x);
  }
  if (ix >= 0x7f800000) return x + x;
  k += (ix >> 23) - 127;
  ix &= 0x007fffff;
  int32_t i = (ix + (0x95f64 << 3)) & 0x800000;
  x = FromWord(ix | (i ^ 0x3f800000));  // normalize x or x/2
  k += (i >> 23);
  const float f = x - 1.0f;
  if ((0x007fffff & (0x8000 + ix)) < 0xc000) {  // -2**-9 <= f < 2**-9
    if (f == 0.0f) {
      if (k == 0) return 0.0f;
      const float dk = float(k);
      return dk * kLn2Hi + dk * kLn2Lo;
    }
    const float r = f * f * (0.5f - 0.33333333333333333f * f);
    if (k == 0) return f - r;
    const float dk = float(k);
    return dk * kLn2Hi - ((r - dk * kLn2Lo) - f);
  }
  const float s = f / (2.0f + f);
  const float dk = float(k);
  const float z = s * s;
  i = ix - (0x6147a << 3);
  const float w = z * z;
  const int32_t j = (0x6b851 << 3) - ix;
  const float t1 = w * (kLg2 + w * kLg4);
  const float t2 = z * (kLg1 + w * kLg3);
  i |= j;
  const float r = t2 + t1;
  if (i > 0) {
    const float hfsq = 0.5f * f * f;
    if (k == 0) return f - (hfsq - s * (hfsq + r));
    return dk * kLn2Hi - ((hfsq - (s * (hfsq + r) + dk * kLn2Lo)) - f);
  }
  if (k == 0) return f - s * (f - r);
  return dk * kLn2Hi - ((s * (f - r) - dk * kLn2Lo) - f);
}

// __ieee754_log10f; Windows x86-64 has float_t == float, so the split
// hi/lo branch below is the one Firefox's build also takes.
float Log10(float x) {
  constexpr float kIvln10Hi = 4.3432617188e-01f, kIvln10Lo = -3.1689971365e-05f;
  constexpr float kLog10_2Hi = 3.0102920532e-01f, kLog10_2Lo = 7.9034151668e-07f;
  constexpr float kTwo25 = 3.3554432000e+07f;
  int32_t hx = Word(x);
  int32_t k = 0;
  if (hx < 0x00800000) {  // x < 2**-126
    if ((hx & 0x7fffffff) == 0) return -kTwo25 / 0.0f;
    if (hx < 0) return (x - x) / 0.0f;
    k -= 25;
    x *= kTwo25;
    hx = Word(x);
  }
  if (hx >= 0x7f800000) return x + x;
  if (hx == 0x3f800000) return 0.0f;  // log(1) = +0
  k += (hx >> 23) - 127;
  hx &= 0x007fffff;
  int32_t i = (hx + 0x4afb0d) & 0x800000;
  x = FromWord(hx | (i ^ 0x3f800000));  // normalize x or x/2
  k += (i >> 23);
  const float y = float(k);
  const float f = x - 1.0f;
  const float hfsq = 0.5f * f * f;
  const float r = Log1p(f);
  float hi = f - hfsq;
  hi = FromWord(Word(hi) & 0xfffff000);
  const float lo = (f - hi) - hfsq + r;
  return y * kLog10_2Lo + (lo + hi) * kIvln10Lo + lo * kIvln10Hi +
         hi * kIvln10Hi + y * kLog10_2Hi;
}

}  // namespace ruyijs::web_audio::dsp
