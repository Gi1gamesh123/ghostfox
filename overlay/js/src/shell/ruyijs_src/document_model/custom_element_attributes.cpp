// attributeChangedCallback and adoption re-keying for custom elements
// (HTML §4.13.6), at the Document Model's committed attribute and rekey points.
#include "document_model/custom_element_reactions.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
using namespace reactions;

void CustomElementAttributeChanged(State& s, NodeKey key, DomStringView name, const DomString* old) {
  CustomElementState* state = s.customElements ? StateOf(s, key) : nullptr;
  if (!Usable(s) || !state || state->kind != CustomElementState::Custom ||
      !state->definition->attributeChanged->isObject()) return;
  bool observed = false;
  for (const auto& entry : state->definition->observed) observed |= DomStringView(entry) == name;
  if (!observed) return;
  std::optional<DomString> current;
  JS::RootedValueArray<4> args(s.cx);
  JSAutoRealm realm(s.cx, s.page);
  if (!s.Check(s.model.GetAttribute(key, name, &current)) || !Value(s.cx, name, args[0]) ||
      !Value(s.cx, current, args[2])) return Settle(s);
  if (old) { if (!Value(s.cx, DomStringView(*old), args[1])) return Settle(s); }
  else args[1].setNull();
  args[3].setNull();
  Invoke(s, key, *state->definition->attributeChanged, args);
}

// A custom element moved to another document keeps its state under the new
// key; the adoptedCallback(old, new) runs at the next committed change point.
void CustomElementsRekey(State& s, NodeKey from, NodeKey to) {
  if (!s.customElements) return;
  auto found = s.customElements->states.find(from);
  if (found == s.customElements->states.end()) return;
  CustomElementState state = found->second;
  s.customElements->states.erase(found);
  NodeSnapshot after;
  const NodeKey previous = state.document;
  if (s.model.Inspect(to, &after).code == ErrorCode::None) state.document = after.owner_document;
  s.customElements->states[to] = state;
  if (state.kind == CustomElementState::Custom && !Same(previous, state.document))
    s.customElements->adopted.push_back({to, {previous, state.document}});
}
}  // namespace ruyijs::document_model
