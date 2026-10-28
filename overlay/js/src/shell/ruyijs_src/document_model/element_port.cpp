#include "document_model/state.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
JSObject* Realm::elementPrototype() {
  Interface* iface = state_->Find("Element");
  return iface ? iface->prototype.get() : nullptr;
}
// The same check without throwing: true when `value` is an Element of any realm.
bool Realm::IsElementValue(JS::HandleValue value, bool* result) {
  *result = false;
  JSContext* cx = state_->cx;
  JS::RootedObject object(cx, value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (!object || JS::GetClass(object) != &kNodeClass) return true;
  auto* wrapper = static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
  if (!wrapper || !wrapper->owner || wrapper->dead) return true;
  State& owner = *wrapper->owner;
  JSAutoRealm realm(cx, owner.page);
  NodeSnapshot node;
  if (!owner.Check(owner.model.Inspect(wrapper->key, &node))) return false;
  *result = node.kind == NodeKind::Element;
  return true;
}
bool Realm::CheckElementReceiver(JS::HandleValue value) {
  JSContext* cx = state_->cx;
  JS::RootedObject object(cx, value.isObject()
      ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (object && JS::GetClass(object) == &kNodeClass) {
    auto* wrapper = static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
    if (wrapper && wrapper->owner && !wrapper->dead) {
      State& owner = *wrapper->owner;
      JSAutoRealm realm(cx, owner.page);
      NodeSnapshot node;
      if (!owner.Check(owner.model.Inspect(wrapper->key, &node))) return false;
      if (node.kind == NodeKind::Element) return true;
    }
  }
  return TypeError(cx, "Element geometry called on an incompatible receiver");
}
}
