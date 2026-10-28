// Attribute attachment stays on the original Element Model and Attr factory.
#include "document_model/state.h"

namespace ruyijs::document_model {
bool FindAttributeByIdentity(State& state, NodeKey element, DomStringView namespace_uri,
                             DomStringView local_name, AttributeSnapshot* result) {
  std::vector<AttributeSnapshot> values;
  if (!state.Check(state.model.Attributes(element, &values))) return false;
  for (const AttributeSnapshot& value : values)
    if (value.namespace_uri == namespace_uri && value.local_name == local_name) {
      *result = value;
      return true;
    }
  return false;
}

bool SetAttributeNode(State& state, NodeKey element, JS::HandleValue value,
                      JS::MutableHandleValue result) {
  AttributeState attribute;
  if (!AttrStateOf(state, value, &attribute))
    return JS_IsExceptionPending(state.cx) ? false : TypeError(state.cx, "setAttributeNode requires an Attr");
  if (!attribute.detached && (attribute.dataOwner != &state || !Same(attribute.owner, element)))
    return ThrowDOM(state, "InUseAttributeError", "The attribute belongs to another element");
  AttributeSnapshot replaced;
  const bool existed =
      FindAttributeByIdentity(state, element, attribute.namespace_uri, attribute.local_name, &replaced);
  if (JS_IsExceptionPending(state.cx)) return false;
  if (attribute.namespace_uri.empty()) {
    if (!state.Check(state.model.SetAttribute(element, attribute.name, attribute.value)))
      return false;
  } else if (!state.Check(state.model.SetAttributeNS(element, attribute.namespace_uri,
                                                     attribute.name, attribute.value))) {
    return false;
  }
  if (!attribute.detached) { result.set(value); return true; }
  if (existed && !DetachedAttributeFor(state, replaced, result)) return false;
  if (!existed) result.setNull();
  return AttrBind(state, value, element);
}
}
