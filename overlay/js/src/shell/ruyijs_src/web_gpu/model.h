#ifndef RUYIJS_WEB_GPU_MODEL_H
#define RUYIJS_WEB_GPU_MODEL_H
#include <cstdint>
#include <string>
#include <vector>

namespace ruyijs::web_gpu {

// Plain mirrors of the imported WebGPU capability owner. model.cpp is the only
// unit that sees domain/webgpu/webgpu_capabilities.h; every other web_gpu unit
// speaks these types and the noexcept facade below.
struct LimitSpec {
  std::string name;
  double value = 0.0;
};

// noexcept facade over ruyijs::domain::webgpu::WebGpuCapabilitiesAggregate. One
// Model lives per browsing realm, so separate pages and iframes never share
// capability state.
class Model {
 public:
  Model() noexcept;
  ~Model();
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  // Replaces the adapter/device feature sets. False when the device set is not
  // a subset of the adapter set; the previous sets survive a rejected change.
  bool Configure(const std::vector<std::string>& adapter,
                 const std::vector<std::string>& device) noexcept;
  // Overrides one existing limit value; false for an unknown name.
  bool SetLimit(const std::string& name, double value) noexcept;
  const std::vector<std::string>& adapter_features() const noexcept;
  const std::vector<std::string>& device_features() const noexcept;
  const std::vector<std::string>& wgsl_features() const noexcept;
  const std::vector<LimitSpec>& limits() const noexcept;
  // True when the name is in the adapter feature set.
  bool AdapterHas(const std::string& feature) const noexcept;

 private:
  struct Impl;
  Impl* impl_;
};

}  // namespace ruyijs::web_gpu
#endif
