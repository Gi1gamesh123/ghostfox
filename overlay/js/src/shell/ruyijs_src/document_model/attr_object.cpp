// The Attr object: one JS object per attribute reference, a live view of one
// attribute of one element, keyed by namespace and local name. createAttribute
// builds a detached Attr whose value lives on the object; setAttributeNode
// binds that same object to the element, after which every field is read from
// the owner. Owner: document_model.
#include "document_model/attr_object.h"


#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
bool Detached(const AttrSlot& slot) { return slot.element.document == 0 && slot.element.node == 0; }
// The owner's current view of the slot's attribute: the identity is fixed, the
// spelling and the value are read live, so a held Attr follows setAttributeNS.
bool Snapshot(State& state, AttrSlot* slot, AttributeSnapshot* result) {
  if (Detached(*slot)) {
    *result = AttributeSnapshot{slot->name, slot->detached_value.value_or(DomString()),
                                slot->namespace_uri, slot->prefix, slot->local_name};
    return true;
  }
  std::vector<AttributeSnapshot> values;
  if (!state.Check(state.model.Attributes(slot->element, &values))) return false;
  *result = AttributeSnapshot{slot->name, DomString(), slot->namespace_uri, DomString(),
                              slot->local_name};
  for (const AttributeSnapshot& value : values)
    if (value.namespace_uri == slot->namespace_uri && value.local_name == slot->local_name) {
      *result = value;
      break;
    }
  return true;
}
bool ValueOf(State& state, AttrSlot* slot, std::optional<DomString>* result) {
  if (Detached(*slot)) {
    *result = slot->detached_value;
    return true;
  }
  if (slot->namespace_uri.empty())
    return state.Check(state.model.GetAttribute(slot->element, slot->name, result));
  return state.Check(state.model.GetAttributeNS(slot->element, slot->namespace_uri,
                                                slot->local_name, result));
}
enum AttrField { Name, Local, Prefix, Namespace, AttrValue, NodeName, NodeType, NodeValue,
                 OwnerElement, Specified };
bool Read(State& state, AttrSlot* slot, AttrField field, JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  if (field == NodeType) { result.setInt32(2); return true; }
  if (field == Specified) { result.setBoolean(true); return true; }
  if (field == OwnerElement) return state.Wrap(slot->element, result);
  if (field == AttrValue || field == NodeValue) {
    std::optional<DomString> value;
    if (!ValueOf(state, slot, &value)) return false;
    return Value(cx, value ? DomStringView(*value) : DomStringView(), result);
  }
  AttributeSnapshot current;
  if (!Snapshot(state, slot, &current)) return false;
  if (field == Name || field == NodeName) return Value(cx, DomStringView(current.name), result);
  if (field == Local) return Value(cx, DomStringView(current.local_name), result);
  if (field == Prefix) {
    if (current.prefix.empty()) { result.setNull(); return true; }
    return Value(cx, DomStringView(current.prefix), result);
  }
  if (current.namespace_uri.empty()) { result.setNull(); return true; }
  return Value(cx, DomStringView(current.namespace_uri), result);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& function = *FunctionState(args);
  const AttrField field = AttrField(FunctionSlot(args));
  JS::RootedObject object(cx, args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  JS::RootedValue receiver(cx, object ? JS::ObjectValue(*object) : JS::NullValue());
  AttrSlot* slot = AttrSlotOf(receiver);
  if (!slot) return TypeError(cx, "Attr receiver is incompatible");
  State& owner = AttrOwner(function, receiver);
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, owner.page);
    if (!Read(owner, slot, field, &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& function = *FunctionState(args);
  JS::RootedObject object(cx, args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  JS::RootedValue receiver(cx, object ? JS::ObjectValue(*object) : JS::NullValue());
  if (!AttrSlotOf(receiver)) return TypeError(cx, "Attr receiver is incompatible");
  DomString value;
  if (!String(cx, args.get(0), &value, true)) return false;
  // Conversion may adopt the element or detach this attribute. Resolve the
  // source wrapper again after the callback, before using any native key.
  AttrSlot* slot = AttrSlotOf(receiver);
  if (!slot) return TypeError(cx, "Attr receiver is incompatible");
  if (Detached(*slot)) {
    slot->detached_value = value;
    args.rval().setUndefined(); return true;
  }
  State& owner = AttrOwner(function, receiver);
  {
    JSAutoRealm realm(cx, owner.page);
    Status status = slot->namespace_uri.empty()
        ? owner.model.SetAttribute(slot->element, slot->name, value)
        : owner.model.SetAttributeNS(slot->element, slot->namespace_uri, slot->name, value);
    if (!owner.Check(status)) return false;
  }
  args.rval().setUndefined(); return true;
}
}  // namespace

bool AttrStateOf(State& state, JS::HandleValue value, AttributeState* result) {
  AttrSlot* slot = AttrSlotOf(value);
  if (!slot) return false;
  State& owner = AttrOwner(state, value);
  std::optional<DomString> current;
  {
    JSAutoRealm realm(state.cx, owner.page);
    if (!ValueOf(owner, slot, &current)) return false;
  }
  result->namespace_uri = slot->namespace_uri;
  result->local_name = slot->local_name;
  result->name = slot->name;
  result->value = current ? *current : DomString();
  result->owner = slot->element;
  result->dataOwner = &owner;
  result->detached = Detached(*slot);
  return true;
}

bool InstallAttr(State& state) {
  static const char* const fields[] = {"name", "value", "localName", "namespaceURI", "prefix",
                                       "specified", "ownerElement", "nodeName", "nodeType",
                                       "nodeValue"};
  static const AttrField slots[] = {Name, AttrValue, Local, Namespace, Prefix, Specified,
                                    OwnerElement, NodeName, NodeType, NodeValue};
  for (size_t index = 0; index < sizeof(fields) / sizeof(fields[0]); ++index) {
    const bool writable = slots[index] == AttrValue || slots[index] == NodeValue;
    if (!state.Accessor("Attr", fields[index], Get, slots[index], writable ? Set : nullptr))
      return false;
  }
  return true;
}
}  // namespace ruyijs::document_model
