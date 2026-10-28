#include "web_audio/compressor.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "web_audio/dsp_math.h"

// Port of dom/media/webaudio/blink/DynamicsCompressorKernel.cpp's main loop: the
// shaped-power detector, the attack/release envelope and the makeup gain, in
// Gecko's evaluation order, including the 32-frame divisions of the loop.
namespace ruyijs::web_audio {
namespace {

constexpr float kPreDelayTime = 0.006f;
constexpr float kPostGain = 0.0f;
constexpr float kEffectBlend = 1.0f;
// Metering releases this fast (in seconds).
constexpr float kMeteringReleaseTimeConstant = 0.325f;
constexpr float kSatReleaseTime = 0.0025f;
constexpr int kDivisionFrames = 32;
constexpr double kPi = 3.14159265358979323846;

}  // namespace

CompressorKernel::CompressorKernel(float sample_rate, unsigned channels)
    : sample_rate_(sample_rate),
      last_pre_delay_frames_(256),
      pre_delay_read_index_(0),
      pre_delay_write_index_(256) {
  SetChannels(channels);
  Reset();
  // DiscreteTimeConstantForSampleRate, kept in double like WebAudioUtils.
  metering_release_k_ = float(1.0 - std::exp(-1.0 / (double(sample_rate) *
                                                    double(kMeteringReleaseTimeConstant))));
}

void CompressorKernel::SetChannels(unsigned channels) {
  if (pre_delay_buffers_.size() == channels) return;
  pre_delay_buffers_.assign(channels, std::array<float, kMaxPreDelayFrames>());
}

void CompressorKernel::SetPreDelayTime(float pre_delay_time) {
  unsigned pre_delay_frames = unsigned(pre_delay_time * sample_rate_);
  if (pre_delay_frames > unsigned(kMaxPreDelayFrames - 1))
    pre_delay_frames = unsigned(kMaxPreDelayFrames - 1);
  if (last_pre_delay_frames_ != pre_delay_frames) {
    last_pre_delay_frames_ = pre_delay_frames;
    for (auto& buffer : pre_delay_buffers_) buffer.fill(0.0f);
    pre_delay_read_index_ = 0;
    pre_delay_write_index_ = int(pre_delay_frames);
  }
}

void CompressorKernel::Process(float* const* source, float* const* destination,
                               unsigned channels, unsigned frames,
                               const CompressorParameters& p) {
  const float dry_mix = 1.0f - kEffectBlend;
  const float wet_mix = kEffectBlend;
  const float k = UpdateStaticCurveParameters(p.threshold, p.knee, p.ratio);
  // Makeup gain.
  const float full_range_gain = Saturate(1.0f, k);
  float full_range_makeup_gain = 1.0f / full_range_gain;
  full_range_makeup_gain = dsp::Pow(full_range_makeup_gain, 0.6f);
  const float master_linear_gain = dsp::ToLinear(kPostGain) * full_range_makeup_gain;
  // Attack parameters.
  const float attack_time = std::max(0.001f, p.attack);
  const float attack_frames = attack_time * sample_rate_;
  // Release parameters.
  const float release_frames = sample_rate_ * p.release;
  const float sat_release_frames = kSatReleaseTime * sample_rate_;
  // A smooth 4th order polynomial through the four release zone points.
  const ReleaseCurve curve = ReleaseCurveFor(release_frames);
  SetPreDelayTime(kPreDelayTime);
  const unsigned divisions = frames / unsigned(kDivisionFrames);
  unsigned frame_index = 0;
  for (unsigned i = 0; i < divisions; ++i) {
    // Calculate desired gain.
    if (std::isnan(detector_average_)) detector_average_ = 1.0f;
    if (std::isinf(detector_average_)) detector_average_ = 1.0f;
    const float desired_gain = detector_average_;
    // Pre-warp so we get desiredGain after the sin() warp below.
    const float scaled_desired_gain =
        float(double(dsp::Asin(desired_gain)) / (0.5 * kPi));
    float envelope_rate;
    const bool is_releasing = scaled_desired_gain > compressor_gain_;
    float compression_diff_db;
    if (scaled_desired_gain == 0.0f) {
      compression_diff_db = std::numeric_limits<float>::infinity();
    } else {
      compression_diff_db = dsp::ToDecibels(compressor_gain_ / scaled_desired_gain,
                                            -1000.0f);
    }
    if (is_releasing) {
      // Release mode - compressionDiffDb should be negative dB.
      max_attack_compression_diff_db_ = -1.0f;
      if (std::isnan(compression_diff_db)) compression_diff_db = -1.0f;
      if (std::isinf(compression_diff_db)) compression_diff_db = -1.0f;
      // Contain within range: -12 -> 0 then scale to go from 0 -> 3.
      float x = compression_diff_db;
      x = std::max(-12.0f, x);
      x = std::min(0.0f, x);
      x = 0.25f * (x + 12);
      const float x2 = x * x;
      const float x3 = x2 * x;
      const float x4 = x2 * x2;
      const float release_frames_at =
          curve.a + curve.b * x + curve.c * x2 + curve.d * x3 + curve.e * x4;
      const float db_per_frame = 5 / release_frames_at;
      envelope_rate = dsp::ToLinear(db_per_frame);
    } else {
      // Attack mode - compressionDiffDb should be positive dB.
      if (std::isnan(compression_diff_db)) compression_diff_db = 1.0f;
      if (std::isinf(compression_diff_db)) compression_diff_db = 1.0f;
      if (max_attack_compression_diff_db_ == -1.0f ||
          max_attack_compression_diff_db_ < compression_diff_db)
        max_attack_compression_diff_db_ = compression_diff_db;
      const float eff_atten_diff_db = std::max(0.5f, max_attack_compression_diff_db_);
      const float x = 0.25f / eff_atten_diff_db;
      envelope_rate = 1 - dsp::Pow(x, 1 / attack_frames);
    }
    // Inner loop - calculate shaped power average - apply compression.
    int pre_delay_read_index = pre_delay_read_index_;
    int pre_delay_write_index = pre_delay_write_index_;
    float detector_average = detector_average_;
    float compressor_gain = compressor_gain_;
    int loop_frames = kDivisionFrames;
    while (loop_frames--) {
      float compressor_input = 0.0f;
      // Pre-delay the signal, computing the compression amount from the
      // undelayed version.
      for (unsigned channel = 0; channel < channels; ++channel) {
        float* delay_buffer = pre_delay_buffers_[channel].data();
        const float undelayed_source = source[channel][frame_index];
        delay_buffer[pre_delay_write_index] = undelayed_source;
        const float abs_undelayed =
            undelayed_source > 0 ? undelayed_source : -undelayed_source;
        if (compressor_input < abs_undelayed) compressor_input = abs_undelayed;
      }
      // Calculate shaped power on the undelayed input.
      const float scaled_input = compressor_input;
      const float abs_input = scaled_input > 0 ? scaled_input : -scaled_input;
      const float shaped_input = Saturate(abs_input, k);
      float attenuation = abs_input <= 0.0001f ? 1.0f : shaped_input / abs_input;
      if (std::isnan(attenuation)) attenuation = 0.0f;
      float attenuation_db = -dsp::ToDecibels(attenuation, -1000.0f);
      attenuation_db = std::max(2.0f, attenuation_db);
      const float db_per_frame = attenuation_db / sat_release_frames;
      const float sat_release_rate = dsp::ToLinear(db_per_frame) - 1;
      const bool is_release = attenuation > detector_average;
      const float rate = is_release ? sat_release_rate : 1;
      detector_average += (attenuation - detector_average) * rate;
      detector_average = std::min(1.0f, detector_average);
      if (std::isnan(detector_average)) detector_average = 1.0f;
      if (std::isinf(detector_average)) detector_average = 1.0f;
      // Exponential approach to the desired gain.
      if (envelope_rate < 1) {
        compressor_gain += (scaled_desired_gain - compressor_gain) * envelope_rate;
      } else {
        compressor_gain *= envelope_rate;
        compressor_gain = std::min(1.0f, compressor_gain);
      }
      // Warp pre-compression gain to smooth out sharp exponential transitions.
      const float post_warp_gain =
          dsp::Sin(float(0.5 * kPi * double(compressor_gain)));
      // Total gain using master gain and effect blend.
      const float total_gain = dry_mix + wet_mix * master_linear_gain * post_warp_gain;
      // Metering.
      const float db_real_gain = 20 * dsp::Log10(post_warp_gain);
      if (db_real_gain < metering_gain_) metering_gain_ = db_real_gain;
      else metering_gain_ += (db_real_gain - metering_gain_) * metering_release_k_;
      // Apply final gain.
      for (unsigned channel = 0; channel < channels; ++channel) {
        float* delay_buffer = pre_delay_buffers_[channel].data();
        destination[channel][frame_index] =
            delay_buffer[pre_delay_read_index] * total_gain;
      }
      frame_index++;
      pre_delay_read_index = (pre_delay_read_index + 1) & (kMaxPreDelayFrames - 1);
      pre_delay_write_index = (pre_delay_write_index + 1) & (kMaxPreDelayFrames - 1);
    }
    pre_delay_read_index_ = pre_delay_read_index;
    pre_delay_write_index_ = pre_delay_write_index;
    detector_average_ = detector_average;
    compressor_gain_ = compressor_gain;
  }
}

void CompressorKernel::Reset() {
  detector_average_ = 0.0f;
  compressor_gain_ = 1.0f;
  metering_gain_ = 1.0f;
  for (auto& buffer : pre_delay_buffers_) buffer.fill(0.0f);
  pre_delay_read_index_ = 0;
  pre_delay_write_index_ = 256;
  max_attack_compression_diff_db_ = -1.0f;
}

}  // namespace ruyijs::web_audio
