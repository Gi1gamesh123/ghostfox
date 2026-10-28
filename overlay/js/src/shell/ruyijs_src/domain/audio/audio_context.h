#pragma once

#include "domain/shared/aggregate_root.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace ruyijs::domain::audio {

using AudioParamId = uint32_t;

struct AudioParamSpec {
    double default_value = 0.0;
    double min_value = 0.0;
    double max_value = 0.0;
    bool a_rate = true;
};

struct AudioParamState {
    AudioParamSpec spec;
    double value = 0.0;
};

class AudioContextAggregate : public shared::AggregateRoot<uint64_t> {
public:
    explicit AudioContextAggregate(uint64_t id = 1)
        : AggregateRoot<uint64_t>(id) {}

    double sample_rate() const noexcept { return sample_rate_; }
    void set_sample_rate(double value) noexcept {
        if (value > 0.0) sample_rate_ = value;
    }

    AudioParamId create_param(const AudioParamSpec& spec) {
        const AudioParamId id = next_param_id_++;
        params_.emplace(id, AudioParamState{spec, spec.default_value});
        return id;
    }

    const AudioParamState* find_param(AudioParamId id) const noexcept {
        const auto it = params_.find(id);
        return it == params_.end() ? nullptr : &it->second;
    }

    bool set_param_value(AudioParamId id, double value) noexcept {
        const auto it = params_.find(id);
        if (it == params_.end()) return false;
        it->second.value = std::clamp(
            value, it->second.spec.min_value, it->second.spec.max_value);
        return true;
    }

    static AudioParamSpec oscillator_frequency(double sample_rate) noexcept {
        return AudioParamSpec{440.0, -sample_rate / 2.0, sample_rate / 2.0, true};
    }

    static AudioParamSpec oscillator_detune() noexcept {
        return AudioParamSpec{0.0, -153600.0, 153600.0, true};
    }

    static AudioParamSpec compressor_threshold() noexcept {
        return AudioParamSpec{-24.0, -100.0, 0.0, false};
    }
    static AudioParamSpec compressor_knee() noexcept {
        return AudioParamSpec{30.0, 0.0, 40.0, false};
    }
    static AudioParamSpec compressor_ratio() noexcept {
        return AudioParamSpec{12.0, 1.0, 20.0, false};
    }
    static AudioParamSpec compressor_attack() noexcept {
        return AudioParamSpec{0.003, 0.0, 1.0, false};
    }
    static AudioParamSpec compressor_release() noexcept {
        return AudioParamSpec{0.25, 0.0, 1.0, false};
    }

private:
    // Firefox's AudioContext default sample rate is the output device rate; the
    // Windows device behind the reference profile runs at 48000 Hz.
    double sample_rate_ = 48000.0;
    AudioParamId next_param_id_ = 1;
    std::unordered_map<AudioParamId, AudioParamState> params_;
};

} // namespace ruyijs::domain::audio
