#include "web_audio/compressor.h"

#include <algorithm>
#include <cmath>

#include "web_audio/dsp_math.h"

// Port of dom/media/webaudio/blink/DynamicsCompressor.cpp and ZeroPole.cpp. The
// pre-emphasis packs are the same four zero/pole stages per channel, the kernel
// runs in place between them, and the emphasis parameters are recomputed on the
// first block only because gain, anchor and ratio never change here.
namespace ruyijs::web_audio {
namespace {

constexpr double kPi = 3.14159265358979323846;

// DynamicsCompressor::initializeParameters constants other than the five node
// parameters.
constexpr float kFilterStageGain = 4.4f;
constexpr float kFilterStageRatio = 2.0f;
constexpr float kFilterAnchorHz = 15000.0f;

}  // namespace

void ZeroPole::Process(const float* source, float* destination, int frames) {
  const float zero = zero_;
  const float pole = pole_;
  // Gain compensation to make 0 dB @ 0 Hz.
  const float k1 = 1.0f / (1.0f - zero);
  const float k2 = 1.0f - pole;
  float last_x = last_x_;
  float last_y = last_y_;
  for (int i = 0; i < frames; ++i) {
    const float input = source[i];
    const float output1 = k1 * (input - zero * last_x);
    last_x = input;
    const float output2 = k2 * output1 + pole * last_y;
    last_y = output2;
    destination[i] = output2;
  }
  last_x_ = last_x;
  last_y_ = last_y;
}

void DynamicsCompressor::Configure(float sample_rate, unsigned channels) {
  sample_rate_ = sample_rate;
  channels_ = channels;
  last_filter_stage_gain_ = -1.0f;  // for recalc
  last_filter_stage_ratio_ = -1.0f;
  last_anchor_ = -1.0f;
  pre_filters_.assign(channels, std::array<ZeroPole, 4>());
  post_filters_.assign(channels, std::array<ZeroPole, 4>());
  kernel_ = CompressorKernel(sample_rate, channels);
}

void DynamicsCompressor::SetEmphasisStage(unsigned stage, float gain,
                                         float normalized_frequency) {
  const float gk = 1.0f - gain / 20.0f;
  const float f1 = normalized_frequency * gk;
  const float f2 = normalized_frequency / gk;
  const float r1 = dsp::Exp(float(-double(f1) * kPi));
  const float r2 = dsp::Exp(float(-double(f2) * kPi));
  for (unsigned i = 0; i < channels_; ++i) {
    // Pre-filter zero/pole create the emphasis; the post-filter reverses them
    // into the matching de-emphasis.
    ZeroPole& pre = pre_filters_[i][stage];
    pre.SetZero(r1);
    pre.SetPole(r2);
    ZeroPole& post = post_filters_[i][stage];
    post.SetZero(r2);
    post.SetPole(r1);
  }
}

void DynamicsCompressor::SetEmphasisParameters(float gain, float anchor_frequency,
                                               float ratio) {
  SetEmphasisStage(0, gain, anchor_frequency);
  SetEmphasisStage(1, gain, anchor_frequency / ratio);
  SetEmphasisStage(2, gain, anchor_frequency / (ratio * ratio));
  SetEmphasisStage(3, gain, anchor_frequency / (ratio * ratio * ratio));
}

void DynamicsCompressor::Process(const float* const* source,
                                 float* const* destination, unsigned channels,
                                 unsigned frames,
                                 const CompressorParameters& parameters) {
  if (channels != channels_) Configure(sample_rate_, channels);
  const float nyquist = sample_rate_ / 2.0f;
  const float filter_stage_gain = kFilterStageGain;
  const float filter_stage_ratio = kFilterStageRatio;
  const float anchor = kFilterAnchorHz / nyquist;
  if (filter_stage_gain != last_filter_stage_gain_ ||
      filter_stage_ratio != last_filter_stage_ratio_ || anchor != last_anchor_) {
    last_filter_stage_gain_ = filter_stage_gain;
    last_filter_stage_ratio_ = filter_stage_ratio;
    last_anchor_ = anchor;
    SetEmphasisParameters(filter_stage_gain, anchor, filter_stage_ratio);
  }
  // Explicit copies: the emphasis stages read and write the same buffer.
  std::vector<std::vector<float>> scratch(channels, std::vector<float>(frames));
  for (unsigned i = 0; i < channels; ++i) {
    const float* data = source[i];
    for (int stage = 0; stage < 4; ++stage) {
      ZeroPole& filter = pre_filters_[i][stage];
      filter.Process(data, scratch[i].data(), int(frames));
      data = scratch[i].data();
    }
    std::copy(data, data + frames, destination[i]);
  }
  kernel_.Process(destination, destination, channels, frames, parameters);
  for (unsigned i = 0; i < channels; ++i) {
    const float* data = destination[i];
    for (int stage = 0; stage < 4; ++stage) {
      ZeroPole& filter = post_filters_[i][stage];
      filter.Process(data, scratch[i].data(), int(frames));
      data = scratch[i].data();
    }
    std::copy(data, data + frames, destination[i]);
  }
}

}  // namespace ruyijs::web_audio
