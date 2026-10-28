#include "web_gpu/model.h"

#include "domain/webgpu/webgpu_capabilities.h"
#include "domain/webgpu/wgsl_language_features.h"

#include <utility>

namespace ruyijs::web_gpu {
namespace {

namespace gpu = ruyijs::domain::webgpu;

const std::vector<std::string>& Empty() {
  static const std::vector<std::string> empty;
  return empty;
}

}  // namespace

struct Model::Impl {
  gpu::WebGpuCapabilitiesAggregate aggregate;
  std::vector<std::string> adapter, device, wgsl;
  std::vector<LimitSpec> limits;
};

Model::Model() noexcept : impl_(nullptr) {
  // The imported aggregate's constructor throws from its shared base, so this
  // is the domain's one exception boundary; nothing escapes the facade.
  try {
    auto* impl = new Impl();
    impl->adapter = impl->aggregate.adapter_features().ordered();
    impl->device = impl->aggregate.device_features().ordered();
    for (const char* feature : gpu::kWgslLanguageFeatures) impl->wgsl.emplace_back(feature);
    for (const auto& limit : impl->aggregate.limits().ordered())
      impl->limits.push_back(LimitSpec{limit.name, limit.value});
    impl_ = impl;
  } catch (...) {
    impl_ = nullptr;
  }
}

Model::~Model() { delete impl_; }

bool Model::Configure(const std::vector<std::string>& adapter,
                      const std::vector<std::string>& device) noexcept {
  if (!impl_) return false;
  try {
    impl_->aggregate.configure(adapter, device);
  } catch (...) {
    return false;
  }
  impl_->adapter = impl_->aggregate.adapter_features().ordered();
  impl_->device = impl_->aggregate.device_features().ordered();
  return true;
}

bool Model::SetLimit(const std::string& name, double value) noexcept {
  if (!impl_) return false;
  for (auto& limit : impl_->limits)
    if (limit.name == name) { limit.value = value; return true; }
  return false;
}

const std::vector<std::string>& Model::adapter_features() const noexcept {
  return impl_ ? impl_->adapter : Empty();
}

const std::vector<std::string>& Model::device_features() const noexcept {
  return impl_ ? impl_->device : Empty();
}

const std::vector<std::string>& Model::wgsl_features() const noexcept {
  return impl_ ? impl_->wgsl : Empty();
}

const std::vector<LimitSpec>& Model::limits() const noexcept {
  static const std::vector<LimitSpec> empty;
  return impl_ ? impl_->limits : empty;
}

bool Model::AdapterHas(const std::string& feature) const noexcept {
  if (!impl_) return false;
  for (const auto& value : impl_->adapter)
    if (value == feature) return true;
  return false;
}

}  // namespace ruyijs::web_gpu
