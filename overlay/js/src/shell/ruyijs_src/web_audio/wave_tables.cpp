#include "web_audio/wave_tables.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "domain/audio/periodic_wave_table.h"
#include "web_audio/dsp_math.h"

// The shape coefficients, the partial culling rules, the table construction and
// the interpolation order come from blink/PeriodicWave.cpp; the phase handling
// comes from OscillatorNode.cpp. The inverse transform itself is the ffvpx
// float RDFT Gecko's FFTBlock runs (domain/audio/periodic_wave_table.h), so the
// band-limited tables round exactly as Firefox's do.
namespace ruyijs::web_audio {
namespace {

constexpr float kPi = 3.14159265358979323846f;

// Amplitude of the sine coefficient the shape's Fourier series needs.
void ShapeCoefficients(WaveTables::Shape shape, unsigned n, float* a, float* b) {
  const float omega = 2.0f * kPi * float(n);
  const float inv_omega = 1.0f / omega;
  *a = 0.0f;
  switch (shape) {
    case WaveTables::kSine:
      *b = n == 1 ? 1.0f : 0.0f;
      break;
    case WaveTables::kSquare:
      *b = inv_omega * ((n & 1) ? 2.0f : 0.0f);
      break;
    case WaveTables::kSawtooth:
      *b = -inv_omega * dsp::Cos(float(0.5 * double(omega)));
      break;
    default: {  // triangle goes max -> min -> max
      *b = 0.0f;
      if (n & 1) {
        *b = 2.0f * (2.0f / (float(n) * kPi) * 2.0f / (float(n) * kPi)) *
             ((((n - 1) >> 1) & 1) ? -1.0f : 1.0f);
      }
      break;
    }
  }
}

}  // namespace

void WaveTables::Configure(float sample_rate, Shape shape) {
  sample_rate_ = sample_rate;
  shape_ = shape;
  const float nyquist = 0.5f * sample_rate_;
  // The constructor's range count, lowest full-partial frequency and rate scale.
  ranges_ = unsigned(3.0f * dsp::Log(float(kSize)) / dsp::Log(2.0f));
  max_partials_ = 0;
  lowest_fundamental_ = nyquist / float(kSize / 2);
  rate_scale_ = float(kSize) / sample_rate_;
  normalization_scale_ = 1.0f;
  Generate(shape);
  tables_.assign(ranges_, std::vector<float>());
  has_tables_ = false;
  lower_ = higher_ = nullptr;
  interpolation_ = 0.0f;
}

void WaveTables::Generate(Shape shape) {
  const unsigned half_size = kSize / 2;
  real_.assign(half_size, 0.0f);
  imag_.assign(half_size, 0.0f);
  real_[0] = 0.0f;
  imag_[0] = 0.0f;
  for (unsigned n = 1; n < half_size; ++n)
    ShapeCoefficients(shape, n, &real_[n], &imag_[n]);
}

unsigned WaveTables::PartialsForRange(unsigned range_index) const {
  // Number of cents below nyquist where partials are culled.
  const float cents_to_cull = float(range_index) * cents_per_range_;
  const float culling_scale = float(std::exp2(double(-cents_to_cull / 1200.0f)));
  return unsigned(culling_scale * float(kSize / 2));
}

void WaveTables::CreateTable(unsigned range_index, float fundamental) {
  unsigned partials = PartialsForRange(range_index);
  partials = std::min(partials, unsigned(real_.size()) - 1);
  const float nyquist = 0.5f * sample_rate_;
  if (fundamental != 0.0f)
    partials = std::min(partials, unsigned(nyquist / fundamental));
  std::vector<float>& table = tables_[range_index];
  table.assign(kSize, 0.0f);
  // The table is the inverse RDFT of bins 1..partials with the conjugate
  // coefficients (RealData = real, ImagData = -imag) and DC cleared, which is
  // what createBandLimitedTables asks FFTBlock::GetInverse for.
  domain::audio::BandLimitedWave(real_.data(), imag_.data(), partials, kSize,
                                 table.data());
  if (range_index == 0) {
    float max_value = 0.0f;
    for (float value : table) max_value = std::max(max_value, std::fabs(value));
    if (max_value) normalization_scale_ = 1.0f / max_value;
  }
  for (float& value : table) value *= normalization_scale_;
}

void WaveTables::Prepare(float fundamental) {
  const float magnitude = std::fabs(fundamental);
  unsigned partials = PartialsForRange(0);
  const float nyquist = 0.5f * sample_rate_;
  if (magnitude != 0.0f)
    partials = std::min(partials, unsigned(nyquist / magnitude));
  if (partials > max_partials_ || !has_tables_) {
    for (auto& table : tables_) table.clear();
    // The first table of a fundamental also fixes the normalization scale.
    CreateTable(0, magnitude);
    max_partials_ = partials;
    has_tables_ = true;
  }
  const float ratio = magnitude > 0 ? magnitude / lowest_fundamental_ : 0.5f;
  const float cents = dsp::Log(ratio) / dsp::Log(2.0f) * 1200.0f;
  // Add one to round up to the next range just in time to truncate partials
  // before aliasing occurs.
  const float pitch_range = 1.0f + cents / cents_per_range_;
  unsigned range1 = unsigned(pitch_range);
  if (range1 >= ranges_) range1 = ranges_ - 1;
  const unsigned range2 = range1 < ranges_ - 1 ? range1 + 1 : range1;
  if (tables_[range1].empty()) CreateTable(range1, magnitude);
  if (tables_[range2].empty()) CreateTable(range2, magnitude);
  lower_ = &tables_[range2];
  higher_ = &tables_[range1];
  interpolation_ = float(range2) - pitch_range;
}

float WaveTables::Next(float* phase, float increment) const {
  const float floor_phase = std::floor(*phase);
  const int32_t j1 = int32_t(floor_phase) & int32_t(kSize - 1);
  const int32_t j2 = (j1 + 1) & int32_t(kSize - 1);
  const float sample_interpolation = *phase - floor_phase;
  const float lower = (1.0f - sample_interpolation) * (*lower_)[size_t(j1)] +
                      sample_interpolation * (*lower_)[size_t(j2)];
  const float higher = (1.0f - sample_interpolation) * (*higher_)[size_t(j1)] +
                       sample_interpolation * (*higher_)[size_t(j2)];
  const float value =
      (1.0f - interpolation_) * lower + interpolation_ * higher;
  // Advance from the wrapped index to avoid losing precision at large phases.
  *phase = float(j1) + sample_interpolation + increment;
  return value;
}

void OscillatorVoice::Configure(float sample_rate, WaveTables::Shape shape) {
  sample_rate_ = sample_rate;
  shape_ = shape;
  tables_.Configure(sample_rate, shape);
  frequency_ = 440.0f;
  final_frequency_ = 0.0f;
  phase_increment_ = 0.0f;
  phase_ = 0.0f;
}

void OscillatorVoice::SetFrequency(float frequency) { frequency_ = frequency; }

void OscillatorVoice::UpdateParameters() {
  if (frequency_ == final_frequency_) return;
  final_frequency_ = frequency_;
  // Gecko divides in double (2 * M_PI is a double there) and stores the result
  // in a float, which the radian phase then accumulates.
  phase_increment_ =
      float(2 * 3.14159265358979323846 * double(final_frequency_) / double(sample_rate_));
  if (shape_ != WaveTables::kSine) tables_.Prepare(final_frequency_);
}

void OscillatorVoice::Compute(float* output, int frames) {
  UpdateParameters();
  if (shape_ == WaveTables::kSine) {
    const float two_pi = float(2 * 3.14159265358979323846);
    for (int i = 0; i < frames; ++i) {
      output[i] = dsp::Sin(phase_);
      phase_ += phase_increment_;
      if (phase_ > two_pi) phase_ -= two_pi;
      else if (phase_ < -two_pi) phase_ += two_pi;
    }
    return;
  }
  const float increment = tables_.rate_scale() * final_frequency_;
  for (int i = 0; i < frames; ++i) output[i] = tables_.Next(&phase_, increment);
}

}  // namespace ruyijs::web_audio
