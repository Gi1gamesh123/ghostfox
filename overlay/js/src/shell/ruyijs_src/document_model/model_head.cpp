#include "document_model/model_internal.h"
#include "domain/document_model/head_values.h"

namespace ruyijs::document_model {
Status Model::HeadAttribute(NodeKey key, DomStringView name, std::optional<DomString>* out) const noexcept {
  return Protect([&] { *out = domain::head_attribute(State().element(key), name); });
}
Status Model::SupportsScriptType(DomStringView type, bool* out) const noexcept {
  return Protect([&] { *out = domain::head_supports_script(type); });
}
}
