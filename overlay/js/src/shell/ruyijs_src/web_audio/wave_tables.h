#ifndef RUYIJS_WEB_AUDIO_WAVE_TABLES_H
#define RUYIJS_WEB_AUDIO_WAVE_TABLES_H
#include <vector>

namespace ruyijs::web_audio {

// Port of dom/media/webaudio/blink/PeriodicWave.cpp for the four standard
// oscillator shapes: one 4096-point band-limited table per pitch range, built
// by an inverse DFT of the shape's Fourier coefficients, peak-normalised from
// the first table of a fundamental, and read back through the bilinear
// two-table interpolation OscillatorNodeEngine::ComputeCustom performs.
class WaveTables {
 public:
  enum Shape : int { kSine = 0, kSquare, kSawtooth, kTriangle };
  static constexpr unsigned kSize = 4096;  // MinPeriodicWaveSize

  void Configure(float sample_rate, Shape shape);
  // Selects (building on demand) the two band-limited tables bracketing
  // |fundamental| and the factor that interpolates between them.
  void Prepare(float fundamental);
  // Bilinear lookup at |phase| (table units, [0, kSize)), advancing the phase by
  // |increment| exactly as the engine does.
  float Next(float* phase, float increment) const;
  float rate_scale() const { return rate_scale_; }

 private:
  void Generate(Shape shape);
  unsigned PartialsForRange(unsigned range_index) const;
  void CreateTable(unsigned range_index, float fundamental);

  float sample_rate_ = 44100.0f;
  Shape shape_ = kSine;
  unsigned ranges_ = 0;
  unsigned max_partials_ = 0;
  float cents_per_range_ = 400.0f;  // CentsPerRange = 1200 / 3
  float lowest_fundamental_ = 0.0f;
  float rate_scale_ = 0.0f;
  float normalization_scale_ = 1.0f;
  bool has_tables_ = false;
  std::vector<float> real_, imag_;
  std::vector<std::vector<float>> tables_;
  const std::vector<float>* lower_ = nullptr;
  const std::vector<float>* higher_ = nullptr;
  float interpolation_ = 0.0f;
};

// OscillatorNodeEngine (dom/media/webaudio/OscillatorNode.cpp): the phase
// accumulator plus the per-shape sample source. The frequency is a constant for
// a render because AudioParam timelines are not played back (D34), so the
// table selection and the phase increment are computed once per frequency.
class OscillatorVoice {
 public:
  void Configure(float sample_rate, WaveTables::Shape shape);
  void SetFrequency(float frequency);
  // Fills |frames| samples of the current render quantum.
  void Compute(float* output, int frames);

 private:
  void UpdateParameters();

  WaveTables tables_;
  WaveTables::Shape shape_ = WaveTables::kSine;
  float sample_rate_ = 44100.0f;
  float frequency_ = 440.0f;
  float final_frequency_ = 0.0f;
  float phase_increment_ = 0.0f;
  float phase_ = 0.0f;  // radians for sine, [0, kSize) table units otherwise
};

}  // namespace ruyijs::web_audio
#endif
