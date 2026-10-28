#include "document_model/model_internal.h"
#include "domain/document_model/image_values.h"

namespace ruyijs::document_model {
Status Model::ImageAttribute(NodeKey key, DomStringView name, std::optional<DomString>* out) const noexcept {
  return Protect([&] { *out = domain::image_attribute(State().element(key), name); });
}
Status Model::ImageSpacing(NodeKey key, DomStringView name, uint32_t* out) const noexcept {
  return Protect([&] { *out = domain::image_spacing(State().element(key), name); });
}
Status Model::SetImageSpacing(NodeKey key, DomStringView name, uint32_t value) noexcept {
  DomString text;
  auto status = Protect([&] { text = domain::from_utf8(std::to_string(value)); });
  return status.code == ErrorCode::None ? SetAttribute(key, name, text) : status;
}
}
