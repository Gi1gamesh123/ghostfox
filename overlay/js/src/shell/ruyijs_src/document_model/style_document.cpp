// Original Document receiver and its traced list reference; no CSS state.
#include "document_model/realm.h"
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
void Realm::SetStyleDocumentListPort(StyleDocumentListPort port, void* context) {
  state_->styleDocumentList = port;state_->styleDocumentContext = context;
}
bool Realm::ResolveStyleSheets(JS::HandleValue value, JS::MutableHandleValue out, bool shadow) {
  JSContext* cx = state_->cx;
  JS::RootedObject receiver(cx, value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  JS::RootedValue branded(cx, receiver ? JS::ObjectValue(*receiver) : value.get());
  Wrapper* entry = shadow ? ShadowReceiver(cx, branded) : DocumentReceiver(cx, receiver);
  if (!entry) return false;
  State& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    result.set(JS::GetReservedSlot(receiver, kNodeStyleAssociationSlot));
    if (!result.isObject()) {
      if (!state.styleDocumentList) return TypeError(cx, "Document CSS factory is not installed");
      if (!state.styleDocumentList(state.styleDocumentContext, receiver, &result)) return false;
      if (!JS_WrapValue(cx, &result)) return false;
      JS::SetReservedSlot(receiver, kNodeStyleAssociationSlot, result);
    }
  }
  if (!JS_WrapValue(cx, &result)) return false;
  out.set(result);
  return true;
}
}
