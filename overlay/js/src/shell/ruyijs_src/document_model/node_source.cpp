// Resolve the current DOM source without reading script-visible properties.
#include "document_model/state.h"
#include "js/Realm.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
Realm* Realm::InputDocument(JS::HandleObject target) {
  JSObject* object = js::CheckedUnwrapStatic(target);
  auto* node = object && JS::GetClass(object) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  return node && node->owner && !node->dead &&
      node->key.document == node->owner->main.document ? node->owner->sourceRealm : nullptr;
}
bool Realm::DocumentParsed() {
  DocumentSnapshot document;
  if (state_->model.InspectDocument(state_->main, &document).code != ErrorCode::None) return true;
  return document.ready_state != ReadyState::Loading;
}
bool Realm::ResolveNodeSource(JS::HandleValue value, NodeSource* result) {
  *result = {};
  State& state = *state_;
  JSAutoRealm realm(state.cx, state.page);
  Wrapper* node = NodeReceiver(state.cx, value);
  if (!node) return false;
  result->owner = node->owner->sourceRealm;
  result->key = node->key;
  result->factory = node->factory ? node->factory->sourceRealm : result->owner;
  return true;
}
}  // namespace ruyijs::document_model
