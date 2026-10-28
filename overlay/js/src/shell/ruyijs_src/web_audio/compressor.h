#ifndef RUYIJS_WEB_AUDIO_COMPRESSOR_H
#define RUYIJS_WEB_AUDIO_COMPRESSOR_H
#include <array>
#include <cstdint>
#include <vector>

// Port of Gecko's DynamicsCompressorNode DSP (dom/media/webaudio/blink:
// ZeroPole.cpp, DynamicsCompressor.cpp, DynamicsCompressorKernel.cpp). The
// render allocates one of these per DynamicsCompressorNode for the duration of
// the render; it holds no JS state and never samples on its own.
namespace ruyijs::web_audio {

// AudioParam values the compressor node forwards per render quantum. The other
// parameters blink keeps (pre-delay 6 ms, post gain 0 dB, effect blend 1, four
// release zones) are the DynamicsCompressor defaults and are constants here.
struct CompressorParameters {
  float threshold = -24.0f;
  float knee = 30.0f;
  float ratio = 12.0f;
  float attack = 0.003f;
  float release = 0.25f;
};

// blink/ZeroPole: one zero and one pole with the 0 dB @ 0 Hz compensation the
// pre/de-emphasis filter packs rely on.
class ZeroPole {
 public:
  void SetZero(float zero) { zero_ = zero; }
  void SetPole(float pole) { pole_ = pole; }
  void Process(const float* source, float* destination, int frames);

 private:
  float zero_ = 0.0f;
  float pole_ = 0.0f;
  float last_x_ = 0.0f;
  float last_y_ = 0.0f;
};

// The adaptive-release polynomial of blink's kernel: y = a + b*x + c*x^2 +
// d*x^3 + e*x^4 through the four release zones of the release time.
struct ReleaseCurve {
  float a = 0.0f;
  float b = 0.0f;
  float c = 0.0f;
  float d = 0.0f;
  float e = 0.0f;
};
ReleaseCurve ReleaseCurveFor(float release_frames);

// blink/DynamicsCompressorKernel: the look-ahead pre-delay, the shaped-power
// detector, the attack/release envelope and the makeup gain, evaluated per
// 32-frame division exactly as the kernel does.
class CompressorKernel {
 public:
  static constexpr int kMaxPreDelayFrames = 1024;

  CompressorKernel(float sample_rate, unsigned channels);
  void Process(float* const* source, float* const* destination, unsigned channels,
               unsigned frames, const CompressorParameters& parameters);
  float metering_gain() const { return metering_gain_; }

 private:
  void Reset();
  void SetChannels(unsigned channels);
  void SetPreDelayTime(float pre_delay_time);
  float KneeCurve(float x, float k);
  float Saturate(float x, float k);
  float SlopeAt(float x, float k);
  float KAtSlope(float desired_slope);
  float UpdateStaticCurveParameters(float db_threshold, float db_knee,
                                    float ratio);

  float sample_rate_;
  float detector_average_ = 0.0f;
  float compressor_gain_ = 1.0f;
  float metering_release_k_ = 0.0f;
  float metering_gain_ = 1.0f;
  unsigned last_pre_delay_frames_;
  int pre_delay_read_index_;
  int pre_delay_write_index_;
  float max_attack_compression_diff_db_ = -1.0f;
  std::vector<std::array<float, kMaxPreDelayFrames>> pre_delay_buffers_;
  float ratio_ = -1.0f;
  float slope_ = -1.0f;
  float linear_threshold_ = -1.0f;
  float db_threshold_ = -1.0f;
  float db_knee_ = -1.0f;
  float knee_threshold_ = -1.0f;
  float knee_threshold_db_ = -1.0f;
  float yknee_threshold_db_ = -1.0f;
  float k_ = -1.0f;
};

// blink/DynamicsCompressor: the four pre-emphasis filters, the kernel and the
// four de-emphasis filters, plus the per-block parameter latch.
class DynamicsCompressor {
 public:
  DynamicsCompressor() = default;
  // A new channel count rebuilds the filter packs, as the engine's
  // ProcessBlock does when the input layout changes.
  void Configure(float sample_rate, unsigned channels);
  void Process(const float* const* source, float* const* destination,
               unsigned channels, unsigned frames,
               const CompressorParameters& parameters);

 private:
  void SetEmphasisStage(unsigned stage, float gain, float normalized_frequency);
  void SetEmphasisParameters(float gain, float anchor_frequency, float ratio);

  float sample_rate_ = 44100.0f;
  unsigned channels_ = 0;
  float last_filter_stage_gain_ = -1.0f;
  float last_filter_stage_ratio_ = -1.0f;
  float last_anchor_ = -1.0f;
  std::vector<std::array<ZeroPole, 4>> pre_filters_;
  std::vector<std::array<ZeroPole, 4>> post_filters_;
  CompressorKernel kernel_{44100.0f, 1};
};

}  // namespace ruyijs::web_audio
#endif
