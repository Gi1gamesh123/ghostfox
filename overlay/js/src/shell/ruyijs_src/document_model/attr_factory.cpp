// The unique Attr object factory, source-node reference and GC lifecycle.
#include "document_model/attr_object.h"
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Realm.h"
#include <new>

namespace ruyijs::document_model {
namespace {
void FinalizeAttr(JS::GCContext*, JSObject* object) {
  delete static_cast<AttrSlot*>(JS::GetReservedSlot(object, 0).toPrivate());
}
const JSClassOps kAttrOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                             FinalizeAttr};
bool Detached(const AttrSlot& slot) {
  return slot.element.document == 0 && slot.element.node == 0;
}
// The namespace prefix of a qualified name; the empty prefix has no colon.
DomString PrefixOf(DomStringView name) {
  const size_t colon = name.find(u':');
  return colon == DomStringView::npos ? DomString() : DomString(name.substr(0, colon));
}
bool Build(State& state, AttrSlot* slot, JS::MutableHandleValue result) {
  JS::RootedObject prototype(state.cx, state.Find("Attr")->prototype);
  JS::RootedObject object(state.cx,
      JS_NewObjectWithGivenProto(state.cx, &kAttrClass, prototype));
  if (!object || !slot) {
    delete slot;
    JS_ReportOutOfMemory(state.cx);
    return false;
  }
  JS::SetReservedSlot(object, 0, JS::PrivateValue(slot));
  if (!Detached(*slot)) {
    JS::RootedValue source(state.cx);
    if (!state.Wrap(slot->element, &source)) return false;
    JS::SetReservedSlot(object, 1, source);
  }
  result.setObject(*object);
  return true;
}
}
const JSClass kAttrClass = {"Attr",
                            JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_FOREGROUND_FINALIZE,
                            &kAttrOps};

bool AttributeValue(State& state, NodeKey element, const DomString& argument,
                    JS::MutableHandleValue result) {
  DomStringView namespace_uri, local_name, name;
  SplitAttributeArgument(argument, &namespace_uri, &local_name, &name);
  return Build(state, new (std::nothrow) AttrSlot{element, DomString(namespace_uri),
                                                  DomString(local_name), DomString(name),
                                                  DomString(), std::nullopt}, result);
}

bool DetachedAttribute(State& state, const DomString& name, JS::MutableHandleValue result) {
  return Build(state, new (std::nothrow) AttrSlot{{}, DomString(), name, name, DomString(),
                                                  DomString()}, result);
}

bool DetachedAttributeFor(State& state, const AttributeSnapshot& value,
                          JS::MutableHandleValue result) {
  return Build(state, new (std::nothrow) AttrSlot{{}, DomString(value.namespace_uri),
                                                  DomString(value.local_name),
                                                  DomString(value.name),
                                                  PrefixOf(value.name),
                                                  DomString(value.value)}, result);
}

bool AttrBind(State& state, JS::HandleValue value, NodeKey element) {
  JS::RootedObject object(state.cx, AttrObject(value));
  AttrSlot* slot = AttrSlotOf(value);
  if (!slot) return TypeError(state.cx, "Attr receiver is incompatible");
  JS::RootedValue source(state.cx);
  if (!state.Wrap(element, &source)) return false;
  JSAutoRealm realm(state.cx, object);
  if (!JS_WrapValue(state.cx, &source)) return false;
  JS::SetReservedSlot(object, 1, source);
  slot->element = element;
  slot->detached_value.reset();
  return true;
}

void AttrDetach(JS::HandleValue value, DomStringView current) {
  AttrSlot* slot = AttrSlotOf(value);
  if (!slot || Detached(*slot)) return;
  slot->detached_value = DomString(current);
  slot->prefix = PrefixOf(slot->name);
  slot->element = NodeKey{};
  JS::SetReservedSlot(AttrObject(value), 1, JS::UndefinedValue());
}

}
