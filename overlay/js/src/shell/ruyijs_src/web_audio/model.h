#ifndef RUYIJS_WEB_AUDIO_MODEL_H
#define RUYIJS_WEB_AUDIO_MODEL_H
#include <cstdint>

namespace ruyijs::web_audio {

// One AudioParam definition, mirrored from the imported ruyijs domain type.
// model.cpp is the only unit that sees the domain header; every other
// web_audio unit speaks this plain struct.
struct ParamSpec {
  double default_value = 0.0;
  double min_value = 0.0;
  double max_value = 0.0;
  bool a_rate = true;
};

// noexcept facade over ruyijs::domain::audio::AudioContextAggregate. The domain
// type owns the sample rate and the parameter table and clamps every written
// value to its [min, max]. One Model lives per browsing realm, so separate
// contexts and iframes never share parameter state.
class Model {
 public:
  Model() noexcept;
  ~Model();
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  double sample_rate() const noexcept;
  uint32_t create_param(const ParamSpec& spec) noexcept;  // 0 when out of memory
  bool param_value(uint32_t id, double* out) const noexcept;
  bool param_default(uint32_t id, double* out) const noexcept;
  bool param_min(uint32_t id, double* out) const noexcept;
  bool param_max(uint32_t id, double* out) const noexcept;
  bool param_is_a_rate(uint32_t id, bool* out) const noexcept;
  bool set_param_value(uint32_t id, double value) noexcept;

  static ParamSpec oscillator_frequency(double sample_rate) noexcept;
  static ParamSpec oscillator_detune() noexcept;
  static ParamSpec compressor_threshold() noexcept;
  static ParamSpec compressor_knee() noexcept;
  static ParamSpec compressor_ratio() noexcept;
  static ParamSpec compressor_attack() noexcept;
  static ParamSpec compressor_release() noexcept;

 private:
  struct Impl;
  Impl* impl_;
};

}  // namespace ruyijs::web_audio
#endif
