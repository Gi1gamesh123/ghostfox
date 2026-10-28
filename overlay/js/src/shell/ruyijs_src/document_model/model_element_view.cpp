#include "document_model/model_internal.h"
#include "domain/document_model/element_view_values.h"

namespace ruyijs::document_model {
Status Model::ElementViewDimension(NodeKey key, bool height, int32_t* out) const noexcept {
  return Protect([&] { *out = domain::element_view_dimension(State().element(key), height); });
}
}
