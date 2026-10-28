#include "document_model/model_internal.h"
#include "domain/document_model/label_tree.h"

namespace ruyijs::document_model {
Status Model::LabelControl(NodeKey key, NodeKey* out) const noexcept {
  return Protect([&] {
    *out = Key(key.document, domain::label_control(State().owner(key), State().element(key)));
  });
}
}
