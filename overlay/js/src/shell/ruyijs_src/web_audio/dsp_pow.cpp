#include "web_audio/dsp_math.h"

#include <bit>
#include <cmath>
#include <cstdint>

// Port of fdlibm __ieee754_powf (modules/fdlibm/src/e_powf.cpp), the routine
// behind fdlibm_powf and therefore behind WebAudioUtils::ConvertDecibelsToLinear
// and the compressor's makeup-gain exponent. Coefficients, the 12/24-bit
// splits and the operation order are kept verbatim.
namespace ruyijs::web_audio::dsp {
namespace {

int32_t Word(float x) { return std::bit_cast<int32_t>(x); }
uint32_t UWord(float x) { return std::bit_cast<uint32_t>(x); }
float FromWord(int32_t bits) { return std::bit_cast<float>(bits); }
float FromUWord(uint32_t bits) { return std::bit_cast<float>(bits); }

constexpr float kOne = 1.0f, kTwo = 2.0f, kHalf = 0.5f, kQrtr = 0.25f;
constexpr float kThrd = 3.33333343e-01f, kTwo24 = 16777216.0f;
constexpr float kHuge = 1.0e30f, kTiny = 1.0e-30f;
constexpr float kL1 = 6.0000002384e-01f, kL2 = 4.2857143283e-01f;
constexpr float kL3 = 3.3333334327e-01f, kL4 = 2.7272811532e-01f;
constexpr float kL5 = 2.3066075146e-01f, kL6 = 2.0697501302e-01f;
constexpr float kP1 = 1.6666667163e-01f, kP2 = -2.7777778450e-03f;
constexpr float kP3 = 6.6137559770e-05f, kP4 = -1.6533901999e-06f;
constexpr float kP5 = 4.1381369442e-08f;
constexpr float kLg2 = 6.9314718246e-01f, kLg2h = 6.93145752e-01f;
constexpr float kLg2l = 1.42860654e-06f, kOvt = 4.2995665694e-08f;
constexpr float kCp = 9.6179670095e-01f, kCph = 9.6191406250e-01f;
constexpr float kCpl = -1.1736857402e-04f, kIvln2 = 1.4426950216e+00f;
constexpr float kIvln2h = 1.4426879883e+00f, kIvln2l = 7.0526075433e-06f;
constexpr float kBp[2] = {1.0f, 1.5f};
constexpr float kDph[2] = {0.0f, 5.84960938e-01f};
constexpr float kDpl[2] = {0.0f, 1.56322085e-06f};

}  // namespace

float Pow(float x, float y) {
  int32_t hx = Word(x), hy = Word(y);
  int32_t ix = hx & 0x7fffffff, iy = hy & 0x7fffffff;
  if (iy == 0) return kOne;                // x**0 = 1
  if (hx == 0x3f800000) return kOne;       // 1**y = 1
  if (ix > 0x7f800000 || iy > 0x7f800000) return x + y;
  // yisint: 0 not an integer, 1 odd, 2 even.
  int32_t yisint = 0;
  if (hx < 0) {
    if (iy >= 0x4b800000) {
      yisint = 2;
    } else if (iy >= 0x3f800000) {
      const int32_t k = (iy >> 23) - 0x7f;
      const int32_t j = iy >> (23 - k);
      if ((j << (23 - k)) == iy) yisint = 2 - (j & 1);
    }
  }
  if (iy == 0x7f800000) {  // y is +-inf
    if (ix == 0x3f800000) return kOne;
    if (ix > 0x3f800000) return hy >= 0 ? y : 0.0f;
    return hy < 0 ? -y : 0.0f;
  }
  if (iy == 0x3f800000) return hy < 0 ? kOne / x : x;
  if (hy == 0x40000000) return x * x;
  if (hy == 0x3f000000) {
    if (hx >= 0) return std::sqrt(x);
  }
  float ax = std::fabs(x);
  if (ix == 0x7f800000 || ix == 0 || ix == 0x3f800000) {
    float z = ax;
    if (hy < 0) z = kOne / z;
    if (hx < 0) {
      if (((ix - 0x3f800000) | yisint) == 0) z = (z - z) / (z - z);
      else if (yisint == 1) z = -z;
    }
    return z;
  }
  int32_t n = int32_t(uint32_t(hx) >> 31) - 1;  // (x<0)**(non-int) is NaN
  if ((n | yisint) == 0) return (x - x) / (x - x);
  float sn = kOne;
  if ((n | (yisint - 1)) == 0) sn = -kOne;

  float t, s_h, s_l, t_h, t_l, z_h, z_l, p_h, p_l;
  if (iy > 0x4d000000) {  // |y| > 2**27
    if (ix < 0x3f7ffff6) return hy < 0 ? sn * kHuge * kHuge : sn * kTiny * kTiny;
    if (ix > 0x3f800007) return hy > 0 ? sn * kHuge * kHuge : sn * kTiny * kTiny;
    t = ax - 1;
    const float w = (t * t) * (kHalf - t * (kThrd - t * kQrtr));
    const float u = kIvln2h * t;
    const float v = t * kIvln2l - w * kIvln2;
    float t1 = u + v;
    t1 = FromWord(Word(t1) & 0xfffff000);
    const float t2 = v - (t1 - u);
    t_h = t1;
    t_l = t2;
  } else {
    int32_t k = 0, j;
    n = 0;
    if (ix < 0x00800000) {
      ax = ax * kTwo24;
      n -= 24;
      ix = Word(ax);
    }
    n += (ix >> 23) - 0x7f;
    j = ix & 0x007fffff;
    ix = j | 0x3f800000;
    if (j <= 0x1cc471) k = 0;
    else if (j < 0x5db3d7) k = 1;
    else {
      k = 0;
      n += 1;
      ix -= 0x00800000;
    }
    const float normalized = FromWord(ix);
    // s = (x-1)/(x+1) or (x-1.5)/(x+1.5).
    const float u = normalized - kBp[k];
    const float v = kOne / (normalized + kBp[k]);
    float s = u * v;
    s_h = s;
    s_h = FromWord(Word(s_h) & 0xfffff000);
    int32_t is = ((ix >> 1) & 0xfffff000) | 0x20000000;
    t_h = FromWord(is + 0x00400000 + (k << 21));
    t_l = normalized - (t_h - kBp[k]);
    s_l = v * ((u - s_h * t_h) - s_h * t_l);
    float s2 = s * s;
    float r = s2 * s2 * (kL1 + s2 * (kL2 + s2 * (kL3 + s2 * (kL4 + s2 * (kL5 + s2 * kL6)))));
    r += s_l * (s_h + s);
    s2 = s_h * s_h;
    t_h = 3 + s2 + r;
    is = Word(t_h);
    t_h = FromWord(is & 0xfffff000);
    t_l = r - ((t_h - 3) - s2);
    const float uu = s_h * t_h;
    const float vv = s_l * t_h + t_l * s;
    p_h = uu + vv;
    p_h = FromWord(Word(p_h) & 0xfffff000);
    p_l = vv - (p_h - uu);
    z_h = kCph * p_h;
    z_l = kCpl * p_h + p_l * kCp + kDpl[k];
    t = float(n);
    float t1 = ((z_h + z_l) + kDph[k]) + t;
    int32_t is2 = Word(t1);
    t1 = FromWord(is2 & 0xfffff000);
    float t2 = z_l - (((t1 - t) - kDph[k]) - z_h);
    t_h = t1;
    t_l = t2;
  }
  // Split up y into y1+y2 and compute (y1+y2)*(t1+t2).
  const float y1 = FromWord(Word(y) & 0xfffff000);
  p_l = (y - y1) * t_h + y * t_l;
  p_h = y1 * t_h;
  float z = p_l + p_h;
  int32_t j = Word(z);
  if (j > 0x43000000) return sn * kHuge * kHuge;
  if (j == 0x43000000) {
    if (p_l + kOvt > z - p_h) return sn * kHuge * kHuge;
  } else if ((j & 0x7fffffff) > 0x43160000) {
    return sn * kTiny * kTiny;
  } else if (j == int32_t(0xc3160000)) {
    if (p_l <= z - p_h) return sn * kTiny * kTiny;
  }
  // Compute 2**(p_h+p_l).
  const int32_t i = j & 0x7fffffff;
  int32_t k = (i >> 23) - 0x7f;
  n = 0;
  if (i > 0x3f000000) {  // |z| > 0.5, set n = [z+0.5]
    n = j + (0x00800000 >> (k + 1));
    k = ((n & 0x7fffffff) >> 23) - 0x7f;
    t = FromWord(n & ~(0x007fffff >> k));
    n = ((n & 0x007fffff) | 0x00800000) >> (23 - k);
    if (j < 0) n = -n;
    p_h -= t;
  }
  t = p_l + p_h;
  t = FromWord(Word(t) & 0xffff8000);
  const float u = t * kLg2h;
  const float v = (p_l - (t - p_h)) * kLg2 + t * kLg2l;
  z = u + v;
  const float w = v - (z - u);
  const float tt = z * z;
  const float t1 = z - tt * (kP1 + tt * (kP2 + tt * (kP3 + tt * (kP4 + tt * kP5))));
  const float r = (z * t1) / (t1 - kTwo) - (w + z * w);
  z = kOne - (r - z);
  int32_t jz = Word(z);
  jz += (n << 23);
  if ((jz >> 23) <= 0) z = float(std::ldexp(double(z), n));
  else z = FromWord(jz);
  return sn * z;
}

}  // namespace ruyijs::web_audio::dsp
