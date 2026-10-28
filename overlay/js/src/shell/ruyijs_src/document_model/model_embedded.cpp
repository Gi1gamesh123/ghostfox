#include "document_model/model_internal.h"
#include "domain/document_model/embedded_values.h"

namespace ruyijs::document_model {
Status Model::EmbeddedInteger(NodeKey key, DomStringView name, double* out) const noexcept {
  return Protect([&] { *out = domain::embedded_integer(State().element(key), name); });
}
Status Model::SetEmbeddedInteger(NodeKey key, DomStringView name, int64_t value) noexcept {
  DomString text;
  auto status = Protect([&] {
    domain::validate_embedded_integer(State().element(key), name, value);
    text = domain::from_utf8(std::to_string(value));
  });
  return status.code == ErrorCode::None ? SetAttribute(key, name, text) : status;
}
}
