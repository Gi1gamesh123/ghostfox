#include "document_model/model_internal.h"
#include "domain/document_model/content_values.h"

namespace ruyijs::document_model {
Status Model::ContentAttribute(NodeKey key, DomStringView name, DomString* out) const noexcept {
  return Protect([&] {
    const auto& e = State().element(key);
    *out = domain::content_attribute(e, name, e.get_attribute16(name).value_or(u""), false);
  });
}
Status Model::SetContentAttribute(NodeKey key, DomStringView name, DomStringView value) noexcept {
  DomString text;
  auto status = Protect([&] { text = domain::content_attribute(State().element(key), name, value, true); });
  return status.code == ErrorCode::None ? SetAttribute(key, name, text) : status;
}
Status Model::ContentInteger(NodeKey key, DomStringView name, int32_t* out) const noexcept {
  return Protect([&] { *out = domain::content_integer(State().element(key), name); });
}
Status Model::SetContentInteger(NodeKey key, DomStringView name, int32_t value) noexcept {
  DomString text;
  auto status = Protect([&] { text = domain::from_utf8(std::to_string(value)); });
  return status.code == ErrorCode::None ? SetAttribute(key, name, text) : status;
}
}
