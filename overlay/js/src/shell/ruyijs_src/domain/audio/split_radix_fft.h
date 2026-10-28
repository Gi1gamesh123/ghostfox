#pragma once

#include <cmath>
#include <vector>

// The float split-radix inverse FFT ffvpx hands to blink's PeriodicWave
// through Gecko's FFTBlock: the "fft" codelet gathers its input with
// ff_tx_gen_ptwo_revtab, then the fft*_ns codelets recurse n -> n/2, n/4, n/4
// and close every stage with ff_tx_fft_sr_combine (libavutil/tx_template.c).
// A Windows x86_64 build selects fft_sr_float_fma3 for that sub transform, so
// the combine's four multiply-adds and the one in the 16-point kernel fuse
// into single roundings; they are spelled out with std::fma below. Every other
// operation keeps the C source's order and float32 rounding, because the
// rounded bits, not the transform, are what has to agree with Gecko.
namespace ruyijs::domain::audio {

struct SpectrumValue {
  float re;
  float im;
};

namespace fft_detail {

// How a complex multiply rounds: ffvpx's fma3 split-radix combine fuses the
// product with the |bre| twiddle for both outputs (kFuseAll), its 16-point
// kernel fuses only the second multiply's real product (kFuseSecondRe), and
// the remaining multiplies round each product on its own (kFuseNone).
enum Fusion { kFuseNone, kFuseSecondRe, kFuseAll };

// BF(x, y, a, b) from tx_priv.h: x = a - b; y = a + b.
inline void Butterfly(float* x, float* y, float a, float b) {
  *x = a - b;
  *y = a + b;
}

// CMUL of tx_priv.h: dre = are*bre - aim*bim, dim = are*bim + aim*bre.
inline void ComplexMultiply(float* dre, float* dim, float are, float aim, float bre,
                            float bim, Fusion fusion) {
  const float re_bim = are * bim;
  const float im_bre = aim * bre;
  if (fusion == kFuseAll) {
    *dre = std::fma(are, bre, -(aim * bim));
    *dim = std::fma(aim, bre, re_bim);
    return;
  }
  const float re_bre = are * bre;
  const float im_bim = aim * bim;
  if (fusion == kFuseSecondRe) {
    *dre = std::fma(are, bre, -im_bim);
    *dim = re_bim + im_bre;
    return;
  }
  *dre = re_bre - im_bim;
  *dim = re_bim + im_bre;
}

// BUTTERFLIES of tx_template.c: |a2|/|a3| fold |t1|,|t2|,|t5|,|t6| into |a0|..|a3|.
inline void Butterflies(SpectrumValue* a0, SpectrumValue* a1, SpectrumValue* a2,
                        SpectrumValue* a3, float t1, float t2, float t5, float t6) {
  const float r0 = a0->re;
  const float i0 = a0->im;
  const float r1 = a1->re;
  const float i1 = a1->im;
  float t3, t4;
  Butterfly(&t3, &t5, t5, t1);
  Butterfly(&a2->re, &a0->re, r0, t5);
  Butterfly(&a3->im, &a1->im, i1, t3);
  Butterfly(&t4, &t6, t2, t6);
  Butterfly(&a3->re, &a1->re, r1, t4);
  Butterfly(&a2->im, &a0->im, i0, t6);
}

// TRANSFORM of tx_template.c.
inline void Transform(SpectrumValue* a0, SpectrumValue* a1, SpectrumValue* a2,
                      SpectrumValue* a3, float wre, float wim, Fusion fusion) {
  float t1, t2, t5, t6;
  const Fusion first = fusion == kFuseAll ? kFuseAll : kFuseNone;
  ComplexMultiply(&t1, &t2, a2->re, a2->im, wre, -wim, first);
  ComplexMultiply(&t5, &t6, a3->re, a3->im, wre, wim, fusion);
  Butterflies(a0, a1, a2, a3, t1, t2, t5, t6);
}

// ff_tx_fft4_ns, in place.
inline void Fft4(SpectrumValue* z) {
  float t1, t2, t3, t4, t5, t6, t7, t8;
  Butterfly(&t3, &t1, z[0].re, z[1].re);
  Butterfly(&t8, &t6, z[3].re, z[2].re);
  Butterfly(&z[2].re, &z[0].re, t1, t6);
  Butterfly(&t4, &t2, z[0].im, z[1].im);
  Butterfly(&t7, &t5, z[2].im, z[3].im);
  Butterfly(&z[3].im, &z[1].im, t4, t8);
  Butterfly(&z[3].re, &z[1].re, t3, t7);
  Butterfly(&z[2].im, &z[0].im, t2, t5);
}

}  // namespace fft_detail

// One stage's cosine twiddles: |out|[i] = cos(i * 2*pi/|n|), |out|[n/4] = 0.
inline std::vector<float> SplitRadixTwiddles(unsigned n) {
  std::vector<float> tab(n / 4 + 1, 0.0f);
  const double step = 2.0 * 3.14159265358979323846 / double(n);
  for (unsigned i = 0; i < n / 4; ++i) tab[i] = float(std::cos(double(i) * step));
  return tab;
}

// ff_tx_gen_ptwo_revtab's split-radix permutation.
inline unsigned SplitRadixIndex(unsigned i, unsigned len, unsigned inv) {
  len >>= 1;
  if (len <= 1) return i & 1u;
  if (!(i & len)) return SplitRadixIndex(i, len, inv) * 2;
  len >>= 1;
  return SplitRadixIndex(i, len, inv) * 4 + 1 - 2 * (((i & len) ? 0u : 1u) ^ inv);
}

// ff_tx_fft8_ns, in place.
inline void SplitRadixFft8(SpectrumValue* z) {
  using namespace fft_detail;  // NOLINT
  const float cos8 = SplitRadixTwiddles(8)[1];
  Fft4(z);
  float t1, t2, t5, t6;
  Butterfly(&t1, &z[5].re, z[4].re, -z[5].re);
  Butterfly(&t2, &z[5].im, z[4].im, -z[5].im);
  Butterfly(&t5, &z[7].re, z[6].re, -z[7].re);
  Butterfly(&t6, &z[7].im, z[6].im, -z[7].im);
  Butterflies(&z[0], &z[2], &z[4], &z[6], t1, t2, t5, t6);
  Transform(&z[1], &z[3], &z[5], &z[7], cos8, cos8, kFuseNone);
}

// ff_tx_fft16_ns, in place.
inline void SplitRadixFft16(SpectrumValue* z) {
  using namespace fft_detail;  // NOLINT
  const std::vector<float> cos16 = SplitRadixTwiddles(16);
  SplitRadixFft8(z);
  Fft4(z + 8);
  Fft4(z + 12);
  const float t1 = z[8].re, t2 = z[8].im, t5 = z[12].re, t6 = z[12].im;
  Butterflies(&z[0], &z[4], &z[8], &z[12], t1, t2, t5, t6);
  Transform(&z[2], &z[6], &z[10], &z[14], cos16[2], cos16[2], kFuseSecondRe);
  Transform(&z[1], &z[5], &z[9], &z[13], cos16[1], cos16[3], kFuseSecondRe);
  Transform(&z[3], &z[7], &z[11], &z[15], cos16[3], cos16[1], kFuseSecondRe);
}

// ff_tx_fft_sr_combine over z[0 .. 8*|len|), the stage that folds the n/2 and
// the two n/4 sub transforms back together.
inline void SplitRadixCombine(SpectrumValue* z, const std::vector<float>& cos,
                              unsigned len) {
  using namespace fft_detail;  // NOLINT
  const unsigned o1 = 2 * len, o2 = 4 * len, o3 = 6 * len;
  const float* tab = cos.data();
  const float* wim = cos.data() + o1 - 7;
  for (unsigned i = 0; i < len; i += 4) {
    Transform(&z[0], &z[o1 + 0], &z[o2 + 0], &z[o3 + 0], tab[0], wim[7], kFuseAll);
    Transform(&z[2], &z[o1 + 2], &z[o2 + 2], &z[o3 + 2], tab[2], wim[5], kFuseAll);
    Transform(&z[4], &z[o1 + 4], &z[o2 + 4], &z[o3 + 4], tab[4], wim[3], kFuseAll);
    Transform(&z[6], &z[o1 + 6], &z[o2 + 6], &z[o3 + 6], tab[6], wim[1], kFuseAll);
    Transform(&z[1], &z[o1 + 1], &z[o2 + 1], &z[o3 + 1], tab[1], wim[6], kFuseAll);
    Transform(&z[3], &z[o1 + 3], &z[o2 + 3], &z[o3 + 3], tab[3], wim[4], kFuseAll);
    Transform(&z[5], &z[o1 + 5], &z[o2 + 5], &z[o3 + 5], tab[5], wim[2], kFuseAll);
    Transform(&z[7], &z[o1 + 7], &z[o2 + 7], &z[o3 + 7], tab[7], wim[0], kFuseAll);
    z += 8;
    tab += 8;
    wim -= 8;
  }
}

// The DECL_SR_CODELET chain: n -> n/2, n/4, n/4 plus the combine, ending on the
// 8- and 16-point kernels. |twiddles| is indexed by stage size.
inline void SplitRadixStage(SpectrumValue* z, unsigned n,
                            const std::vector<std::vector<float>>& twiddles) {
  if (n == 8) {
    SplitRadixFft8(z);
    return;
  }
  if (n == 16) {
    SplitRadixFft16(z);
    return;
  }
  const unsigned half = n / 2, quarter = n / 4;
  SplitRadixStage(z, half, twiddles);
  SplitRadixStage(z + 2 * quarter, quarter, twiddles);
  SplitRadixStage(z + 3 * quarter, quarter, twiddles);
  SplitRadixCombine(z, twiddles[n], quarter >> 1);
}

// The ff_tx_fft codelet plus the split-radix network above: the inverse
// transform of |n| complex samples, gathered into the codelet's order first.
inline void InverseComplexFft(const SpectrumValue* src, SpectrumValue* dst, unsigned n) {
  std::vector<std::vector<float>> twiddles(n + 1);
  for (unsigned m = 8; m <= n; m <<= 1) twiddles[m] = SplitRadixTwiddles(m);
  for (unsigned i = 0; i < n; ++i) {
    const unsigned reversed = ~SplitRadixIndex(i, n, 1u) + 1u;  // -permutation
    dst[i] = src[reversed & (n - 1)];
  }
  SplitRadixStage(dst, n, twiddles);
}

}  // namespace ruyijs::domain::audio
