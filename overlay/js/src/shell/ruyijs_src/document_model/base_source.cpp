#include "document_model/state.h"

namespace ruyijs::document_model {
bool Realm::ReadBaseReference(JS::MutableHandleValue result) {
  State& state = *state_;
  std::vector<NodeKey> nodes;
  if (!state.Check(state.model.QuerySelector(state.main, u"base[href]", &nodes))) return false;
  std::optional<DomString> href;
  if (!nodes.empty() && !state.Check(state.model.GetAttribute(nodes.front(), u"href", &href)))
    return false;
  return Value(state.cx, href, result);
}
}
