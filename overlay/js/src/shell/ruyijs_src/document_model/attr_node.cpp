// Element's attribute-node members (T6, A6.8): getAttributeNode/getAttributeNodeNS,
// setAttributeNode/setAttributeNodeNS and removeAttributeNode. They exchange Attr
// objects with their owner (attr_object.cpp) instead of holding attribute state.
#include "document_model/state.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum NodeMethod { GetNode, GetNodeNS, SetNode, SetNodeNS, RemoveNode };

DomString Folded(DomStringView name) {
  DomString out;
  out.reserve(name.size());
  for (char16_t c : name) out.push_back(c >= u'A' && c <= u'Z' ? char16_t(c + 32) : c);
  return out;
}

bool List(State& state, NodeKey element, std::vector<AttributeSnapshot>* values) {
  return state.Check(state.model.Attributes(element, values));
}

// The plain family looks a qualified name up the way get_attribute16 does: the
// exact spelling first, then the HTML-folded one (the caller cannot fold here
// without the document mode, so both spellings are tried).
bool FindByName(State& state, NodeKey element, DomStringView name, AttributeSnapshot* result) {
  std::vector<AttributeSnapshot> values;
  if (!List(state, element, &values)) return false;
  const AttributeSnapshot* found = nullptr;
  for (const AttributeSnapshot& value : values)
    if (value.name == name) { found = &value; break; }
  if (!found) {
    const DomString folded = Folded(name);
    for (const AttributeSnapshot& value : values)
      if (Folded(value.name) == folded) { found = &value; break; }
  }
  if (!found) return false;
  *result = *found;
  return true;
}

bool GetNodeCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = ElementReceiver(cx, args.thisv());
  if (!entry) return false;
  const bool namespaced = FunctionSlot(args) == GetNodeNS;
  if (argc < (namespaced ? 2u : 1u))
    return TypeError(cx, "getAttributeNode has too few arguments");
  DomString namespace_uri, local_name;
  if (!String(cx, args[0], namespaced ? &namespace_uri : &local_name, namespaced)) return false;
  if (namespaced && !String(cx, args[1], &local_name)) return false;
  JS::RootedValue result(cx);
  {
    State& state = *entry->owner;
    JSAutoRealm realm(cx, state.page);
    AttributeSnapshot found;
    const bool present = namespaced
        ? FindAttributeByIdentity(state, entry->key, namespace_uri, local_name, &found)
        : FindByName(state, entry->key, local_name, &found);
    if (!present) {
      if (JS_IsExceptionPending(cx)) return false;
      args.rval().setNull(); return true;
    }
    if (!AttributeValue(state, entry->key,
          AttributeArgument(found.namespace_uri, found.local_name, found.name), &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}

bool SetNodeCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = ElementReceiver(cx, args.thisv());
  if (!entry) return false;
  if (!argc) return TypeError(cx, "setAttributeNode has too few arguments");
  JS::RootedValue value(cx, args[0]), result(cx);
  {
    State& state = *entry->owner;
    JSAutoRealm realm(cx, state.page);
    if (!JS_WrapValue(cx, &value) || !SetAttributeNode(state, entry->key, value, &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}

bool RemoveNodeCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = ElementReceiver(cx, args.thisv());
  if (!entry) return false;
  if (!argc) return TypeError(cx, "removeAttributeNode has too few arguments");
  State& state = *entry->owner;
  AttributeState attribute;
  if (!AttrStateOf(state, args.get(0), &attribute))
    return TypeError(cx, "removeAttributeNode requires an Attr");
  if (attribute.detached || attribute.dataOwner != &state || !Same(attribute.owner, entry->key))
    return ThrowDOM(state, "NotFoundError", "The attribute is not on this element");
  AttributeSnapshot present;
  if (!FindAttributeByIdentity(state, entry->key, attribute.namespace_uri, attribute.local_name, &present))
    return ThrowDOM(state, "NotFoundError", "The attribute is not on this element");
  if (attribute.namespace_uri.empty()) {
    if (!state.Check(state.model.RemoveAttribute(entry->key, attribute.name))) return false;
  } else if (!state.Check(state.model.RemoveAttributeNS(entry->key, attribute.namespace_uri,
                                                        attribute.local_name))) {
    return false;
  }
  AttrDetach(args.get(0), attribute.value);
  args.rval().set(args[0]);
  return true;
}
}  // namespace

bool InstallAttrNode(State& state) {
  return state.Method("Element", "getAttributeNode", GetNodeCall, 1, GetNode) &&
      state.Method("Element", "getAttributeNodeNS", GetNodeCall, 2, GetNodeNS) &&
      state.Method("Element", "setAttributeNode", SetNodeCall, 1, SetNode) &&
      state.Method("Element", "setAttributeNodeNS", SetNodeCall, 1, SetNodeNS) &&
      state.Method("Element", "removeAttributeNode", RemoveNodeCall, 1, RemoveNode);
}
}  // namespace ruyijs::document_model
