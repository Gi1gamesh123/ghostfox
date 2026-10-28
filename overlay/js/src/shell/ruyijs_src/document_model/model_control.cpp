#include "document_model/model_internal.h"
#include "domain/document_model/button_values.h"

namespace ruyijs::document_model {
Status Model::CustomValidity(NodeKey key, DomString* out) const noexcept {
  return Protect([&] { *out = State().element(key).custom_validity(); });
}
Status Model::SetCustomValidity(NodeKey key, DomStringView value) noexcept {
  return Protect([&] {
    auto& element = State().element(key);
    State().budget(0, value.size(), element.custom_validity().size());
    element.set_custom_validity(DomString(value));
  });
}
Status Model::ButtonAttribute(NodeKey key, DomStringView name, DomString* out) const noexcept {
  return Protect([&] {
    *out = domain::button_attribute(name, State().element(key).get_attribute16(name).value_or(u""));
  });
}
}
