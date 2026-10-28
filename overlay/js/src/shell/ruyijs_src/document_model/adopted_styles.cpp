// Original DOM brands, scope identity and a traced CSS factory association.
#include "document_model/realm.h"
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
void Realm::SetAdoptedStylesListPort(StyleDocumentListPort port, void* context) {
  state_->adoptedStylesList = port; state_->adoptedStylesContext = context;
}
bool Realm::ResolveAdoptedStyles(JS::HandleValue value, JS::MutableHandleValue out, bool shadow) {
  JSContext* cx = state_->cx;
  JS::RootedObject receiver(cx, value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  JS::RootedValue branded(cx, receiver ? JS::ObjectValue(*receiver) : value.get());
  Wrapper* entry = shadow ? ShadowReceiver(cx, branded) : DocumentReceiver(cx, receiver);
  if (!entry) return false;
  State& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    result.set(JS::GetReservedSlot(receiver, kNodeAdoptedStylesSlot));
    if (!result.isObject()) {
      if (!state.adoptedStylesList) return TypeError(cx, "Adopted CSS factory is not installed");
      if (!state.adoptedStylesList(state.adoptedStylesContext, receiver, &result) ||
          !JS_WrapValue(cx, &result)) return false;
      JS::SetReservedSlot(receiver, kNodeAdoptedStylesSlot, result);
    }
  }
  if (!JS_WrapValue(cx, &result)) return false;
  out.set(result); return true;
}
bool Realm::ReadStyleScopeDocument(JS::HandleObject scope, JS::MutableHandleObject out) {
  JSContext* cx = state_->cx;
  JS::RootedObject receiver(cx, js::CheckedUnwrapStatic(scope));
  JS::RootedValue branded(cx, receiver ? JS::ObjectValue(*receiver) : JS::NullValue());
  Wrapper* entry = NodeReceiver(cx, branded);
  if (!entry) return false;
  JS::RootedValue document(cx);
  {
    State& state = *entry->owner;
    JSAutoRealm realm(cx, state.page);
    NodeSnapshot snapshot;
    if (!state.Check(state.model.Inspect(entry->key, &snapshot)) ||
        !state.Wrap(snapshot.kind == NodeKind::Document ? entry->key : snapshot.owner_document,
                    &document)) return false;
  }
  if (!JS_WrapValue(cx, &document)) return false;
  out.set(&document.toObject()); return true;
}
bool Realm::ReadCascadeScope(JS::HandleObject element, JS::MutableHandleObject out, bool* shadow) {
  JSContext* cx = state_->cx;
  JS::RootedObject receiver(cx, js::CheckedUnwrapStatic(element));
  JS::RootedValue branded(cx, receiver ? JS::ObjectValue(*receiver) : JS::NullValue());
  Wrapper* entry = NodeReceiver(cx, branded);
  if (!entry) return false;
  JS::RootedValue scope(cx);
  {
    State& state = *entry->owner;
    JSAutoRealm realm(cx, state.page);
    NodeSnapshot node, root;
    if (!state.Check(state.model.Inspect(entry->key, &node)) ||
        !state.Check(state.model.Inspect(node.root, &root))) return false;
    *shadow = root.shadow_host.node != 0;
    NodeKey key = *shadow || root.kind == NodeKind::Document ? node.root : node.owner_document;
    if (!state.Wrap(key, &scope)) return false;
  }
  if (!JS_WrapValue(cx, &scope)) return false;
  out.set(&scope.toObject()); return true;
}
}
