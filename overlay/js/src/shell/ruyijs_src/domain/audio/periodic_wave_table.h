#pragma once

#include <cmath>
#include <cstring>
#include <vector>

#include "domain/audio/split_radix_fft.h"

// PeriodicWave::createBandLimitedTables (dom/media/webaudio/blink/PeriodicWave.cpp)
// driven through FFTBlock::GetInverse: the inverse real DFT of a PeriodicWave's
// Fourier coefficients, i.e. ffvpx's AV_TX_FLOAT_RDFT inverse at FFTBlock's unit
// inverse scaling. ff_tx_rdft_c2r folds the conjugate bins and scales them by
// the +-1/+-2 factors ff_tx_rdft_init derives from scale 1.0f; the real output
// is then the fft_sr_float_fma3 sub transform of the folded spectrum.
namespace ruyijs::domain::audio {

namespace wave_detail {

// MULT(fact[i], x) from ff_tx_rdft_init's table for scale 1.0f.
inline float Scaled(float factor, float value) { return factor * value; }

}  // namespace wave_detail

// Fills |table| (|size| samples) with the band-limited waveform of the |real| /
// |imag| Fourier coefficients, keeping partials 1..|partials| and dropping the
// DC term exactly as createBandLimitedTables does before normalisation.
inline void BandLimitedWave(const float* real, const float* imag, unsigned partials,
                            unsigned size, float* table) {
  using wave_detail::Scaled;
  const unsigned half = size / 2;
  const unsigned quarter = size / 4;
  partials = partials < half ? partials : half - 1;

  // The coefficients are copied with a complex conjugate, and bins above the
  // band limit stay zero; then DC is cleared (it has no effect either way).
  std::vector<SpectrumValue> spectrum(half + 1);
  for (unsigned i = 1; i <= partials; ++i) {
    spectrum[i].re = real[i];
    spectrum[i].im = -imag[i];
  }
  spectrum[0].re = 0.0f;
  spectrum[0].im = 0.0f;
  spectrum[half].re = 0.0f;
  spectrum[half].im = 0.0f;

  // ff_tx_rdft_c2r for inv == 1: the last bin's real part is the DC imaginary
  // part, DC becomes re/im = (re + im), (re - im), and both are scaled by 1.
  spectrum[0].im = spectrum[half].re;
  const float dc = spectrum[0].re;
  spectrum[0].re = Scaled(1.0f, dc + spectrum[0].im);
  spectrum[0].im = Scaled(1.0f, dc - spectrum[0].im);
  spectrum[quarter].re = Scaled(2.0f, spectrum[quarter].re);
  spectrum[quarter].im = Scaled(-2.0f, spectrum[quarter].im);

  // The bin pairs are separated into even and odd parts, the odd part is
  // rotated by the stage twiddles (tcos = cos(i*f), tsin = cos((quarter - i)*f))
  // and both are folded back.
  const double step = 2.0 * 3.14159265358979323846 / double(size);
  for (unsigned i = 1; i < quarter; ++i) {
    const float tcos = float(std::cos(double(i) * step));
    const float tsin = float(std::cos(double(quarter - i) * step));
    const SpectrumValue low = spectrum[i];
    const SpectrumValue high = spectrum[half - i];
    const float even_re = Scaled(1.0f, low.re + high.re);
    const float even_im = Scaled(-1.0f, low.im - high.im);
    const float odd_re = Scaled(-1.0f, low.im + high.im);
    const float odd_im = Scaled(1.0f, low.re - high.re);
    float turned_re, turned_im;
    fft_detail::ComplexMultiply(&turned_re, &turned_im, odd_re, odd_im, tcos, tsin,
                                fft_detail::kFuseNone);
    spectrum[i].re = even_re + turned_re;
    spectrum[i].im = turned_im - even_im;
    spectrum[half - i].re = even_re - turned_re;
    spectrum[half - i].im = turned_im + even_im;
  }

  std::vector<SpectrumValue> out(half);
  InverseComplexFft(spectrum.data(), out.data(), half);
  static_assert(sizeof(SpectrumValue) == 2 * sizeof(float),
                "the time-domain table is a flat float array");
  std::memcpy(table, out.data(), size_t(size) * sizeof(float));
}

}  // namespace ruyijs::domain::audio
