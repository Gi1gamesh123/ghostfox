#include "web_audio/model.h"

#include "domain/audio/audio_context.h"

namespace ruyijs::web_audio {
namespace {

ruyijs::domain::audio::AudioParamSpec ToDomain(const ParamSpec& spec) {
  return ruyijs::domain::audio::AudioParamSpec{
      spec.default_value, spec.min_value, spec.max_value, spec.a_rate};
}

ParamSpec FromDomain(const ruyijs::domain::audio::AudioParamSpec& spec) {
  return ParamSpec{spec.default_value, spec.min_value, spec.max_value, spec.a_rate};
}

}  // namespace

struct Model::Impl {
  ruyijs::domain::audio::AudioContextAggregate aggregate;
};

Model::Model() noexcept : impl_(nullptr) {
  try {
    impl_ = new Impl();
  } catch (...) {
    impl_ = nullptr;
  }
}

Model::~Model() { delete impl_; }

double Model::sample_rate() const noexcept {
  return impl_ ? impl_->aggregate.sample_rate() : 48000.0;
}

uint32_t Model::create_param(const ParamSpec& spec) noexcept {
  if (!impl_) return 0;
  try {
    return impl_->aggregate.create_param(ToDomain(spec));
  } catch (...) {
    return 0;
  }
}

bool Model::param_value(uint32_t id, double* out) const noexcept {
  const auto* state = impl_ ? impl_->aggregate.find_param(id) : nullptr;
  if (!state) return false;
  *out = state->value;
  return true;
}

bool Model::param_default(uint32_t id, double* out) const noexcept {
  const auto* state = impl_ ? impl_->aggregate.find_param(id) : nullptr;
  if (!state) return false;
  *out = state->spec.default_value;
  return true;
}

bool Model::param_min(uint32_t id, double* out) const noexcept {
  const auto* state = impl_ ? impl_->aggregate.find_param(id) : nullptr;
  if (!state) return false;
  *out = state->spec.min_value;
  return true;
}

bool Model::param_max(uint32_t id, double* out) const noexcept {
  const auto* state = impl_ ? impl_->aggregate.find_param(id) : nullptr;
  if (!state) return false;
  *out = state->spec.max_value;
  return true;
}

bool Model::param_is_a_rate(uint32_t id, bool* out) const noexcept {
  const auto* state = impl_ ? impl_->aggregate.find_param(id) : nullptr;
  if (!state) return false;
  *out = state->spec.a_rate;
  return true;
}

bool Model::set_param_value(uint32_t id, double value) noexcept {
  return impl_ && impl_->aggregate.set_param_value(id, value);
}

ParamSpec Model::oscillator_frequency(double sample_rate) noexcept {
  return FromDomain(ruyijs::domain::audio::AudioContextAggregate::oscillator_frequency(
      sample_rate));
}

ParamSpec Model::oscillator_detune() noexcept {
  return FromDomain(ruyijs::domain::audio::AudioContextAggregate::oscillator_detune());
}

ParamSpec Model::compressor_threshold() noexcept {
  return FromDomain(ruyijs::domain::audio::AudioContextAggregate::compressor_threshold());
}

ParamSpec Model::compressor_knee() noexcept {
  return FromDomain(ruyijs::domain::audio::AudioContextAggregate::compressor_knee());
}

ParamSpec Model::compressor_ratio() noexcept {
  return FromDomain(ruyijs::domain::audio::AudioContextAggregate::compressor_ratio());
}

ParamSpec Model::compressor_attack() noexcept {
  return FromDomain(ruyijs::domain::audio::AudioContextAggregate::compressor_attack());
}

ParamSpec Model::compressor_release() noexcept {
  return FromDomain(ruyijs::domain::audio::AudioContextAggregate::compressor_release());
}

}  // namespace ruyijs::web_audio
