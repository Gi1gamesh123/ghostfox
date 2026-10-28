// The original node's traced association, not an independent CSS object store.
#include "document_model/realm.h"
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
void Realm::SetStyleElementPort(StyleElementPort port, void* context) {
  state_->styleElement = port;state_->styleContext = context;
}
bool Realm::AssociateStyleSheet(JS::HandleObject node, JS::HandleValue expected,
                                JS::HandleValue replacement) {
  JSContext* cx = state_->cx;
  JS::RootedValue receiver(cx, JS::ObjectValue(*node));
  Wrapper* entry = NodeReceiver(cx, receiver);
  if (!entry) return false;
  JSAutoRealm realm(cx, entry->owner->page);
  JS::RootedObject self(cx, entry->object);
  JS::RootedValue old(cx, expected),value(cx, replacement);
  if (!JS_WrapValue(cx, &old) || !JS_WrapValue(cx, &value)) return false;
  const auto& current = JS::GetReservedSlot(self, kNodeStyleAssociationSlot);
  if (old.isUndefined() || (old.isObject() && current.isObject() &&
      &current.toObject() == &old.toObject()))
    JS::SetReservedSlot(self, kNodeStyleAssociationSlot, value);
  return true;
}
}
