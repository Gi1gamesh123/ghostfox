#pragma once

#include "domain/shared/aggregate_root.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ruyijs::domain::webgpu {

class SupportedFeatures {
public:
    const std::vector<std::string>& ordered() const noexcept { return ordered_; }

    bool contains(const std::string& feature) const noexcept {
        return values_.count(feature) != 0;
    }

    void replace(std::vector<std::string> features) {
        std::unordered_set<std::string> values;
        values.reserve(features.size());
        for (const auto& feature : features) {
            if (feature.empty() || !values.insert(feature).second) {
                throw std::invalid_argument("invalid WebGPU feature set");
            }
        }
        ordered_ = std::move(features);
        values_ = std::move(values);
    }

private:
    std::vector<std::string> ordered_;
    std::unordered_set<std::string> values_;
};

// The GPUSupportedLimits table. The donor aggregate carried no limits, so the
// table lives in this owner: the WebGPU adapter reads it through the noexcept
// facade and never writes a limit literal of its own (T19, deviation D39).
// Values are doubles because the specification's maxima exceed 32 bits
// (maxBufferSize, maxStorageBufferBindingSize).
struct GpuLimit {
    std::string name;
    double value = 0.0;
};

// The specification's default limits, in GPUSupportedLimits member order.
inline std::vector<GpuLimit> DefaultGpuLimits() {
    return {
        {"maxTextureDimension1D", 8192.0},
        {"maxTextureDimension2D", 8192.0},
        {"maxTextureDimension3D", 2048.0},
        {"maxTextureArrayLayers", 256.0},
        {"maxBindGroups", 4.0},
        {"maxBindGroupsPlusVertexBuffers", 24.0},
        {"maxBindingsPerBindGroup", 1000.0},
        {"maxDynamicUniformBuffersPerPipelineLayout", 8.0},
        {"maxDynamicStorageBuffersPerPipelineLayout", 4.0},
        {"maxSampledTexturesPerShaderStage", 16.0},
        {"maxSamplersPerShaderStage", 16.0},
        {"maxStorageBuffersPerShaderStage", 8.0},
        {"maxStorageTexturesPerShaderStage", 4.0},
        {"maxUniformBuffersPerShaderStage", 12.0},
        {"maxUniformBufferBindingSize", 65536.0},
        {"maxStorageBufferBindingSize", 134217728.0},
        {"minUniformBufferOffsetAlignment", 256.0},
        {"minStorageBufferOffsetAlignment", 256.0},
        {"maxVertexBuffers", 8.0},
        {"maxBufferSize", 268435456.0},
        {"maxVertexAttributes", 16.0},
        {"maxVertexBufferArrayStride", 2048.0},
        {"maxInterStageShaderVariables", 16.0},
        {"maxColorAttachments", 8.0},
        {"maxColorAttachmentBytesPerSample", 32.0},
        {"maxComputeWorkgroupStorageSize", 16384.0},
        {"maxComputeInvocationsPerWorkgroup", 256.0},
        {"maxComputeWorkgroupSizeX", 256.0},
        {"maxComputeWorkgroupSizeY", 256.0},
        {"maxComputeWorkgroupSizeZ", 64.0},
        {"maxComputeWorkgroupsPerDimension", 65535.0},
    };
}

class LimitTable {
public:
    const std::vector<GpuLimit>& ordered() const noexcept { return ordered_; }

    bool contains(const std::string& name) const noexcept {
        for (const auto& limit : ordered_) {
            if (limit.name == name) return true;
        }
        return false;
    }

    double value(const std::string& name) const noexcept {
        for (const auto& limit : ordered_) {
            if (limit.name == name) return limit.value;
        }
        return 0.0;
    }

    // Replaces the whole table. An empty or duplicate name, or a negative or
    // non-finite value, is rejected instead of silently producing a nonsensical
    // limit.
    void replace(std::vector<GpuLimit> limits) {
        std::vector<GpuLimit> next;
        next.reserve(limits.size());
        for (auto& limit : limits) {
            if (limit.name.empty() || !std::isfinite(limit.value) ||
                limit.value < 0.0) {
                throw std::invalid_argument("invalid WebGPU limit");
            }
            for (const auto& seen : next) {
                if (seen.name == limit.name) {
                    throw std::invalid_argument("duplicate WebGPU limit");
                }
            }
            next.push_back(std::move(limit));
        }
        ordered_ = std::move(next);
    }

private:
    std::vector<GpuLimit> ordered_ = DefaultGpuLimits();
};

class WebGpuCapabilitiesAggregate : public shared::AggregateRoot<uint64_t> {
public:
    explicit WebGpuCapabilitiesAggregate(uint64_t id = 1)
        : AggregateRoot<uint64_t>(id) {}

    const SupportedFeatures& adapter_features() const noexcept {
        return adapter_features_;
    }
    const SupportedFeatures& device_features() const noexcept {
        return device_features_;
    }

    const LimitTable& limits() const noexcept { return limits_; }

    void configure_adapter_features(std::vector<std::string> features) {
        adapter_features_.replace(std::move(features));
    }
    void configure_device_features(std::vector<std::string> features) {
        device_features_.replace(std::move(features));
    }

    void configure(std::vector<std::string> adapter,
                   std::vector<std::string> device) {
        SupportedFeatures next_adapter;
        SupportedFeatures next_device;
        next_adapter.replace(std::move(adapter));
        next_device.replace(std::move(device));
        for (const auto& feature : next_device.ordered()) {
            if (!next_adapter.contains(feature)) {
                throw std::invalid_argument(
                    "WebGPU device feature is not supported by adapter");
            }
        }
        adapter_features_ = std::move(next_adapter);
        device_features_ = std::move(next_device);
    }

private:
    SupportedFeatures adapter_features_;
    SupportedFeatures device_features_;
    LimitTable limits_;
};

} // namespace ruyijs::domain::webgpu
