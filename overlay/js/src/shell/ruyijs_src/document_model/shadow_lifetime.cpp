#include "document_model/state.h"
#include "js/Object.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
bool RefreshShadowScope(State& state, NodeKey key, JS::HandleObject object) {
  NodeKey scope;
  if (state.model.ShadowScope(key, &scope).code != ErrorCode::None) return true;
  Wrapper* entry = scope.node ? state.Find(scope) : nullptr;
  JS::RootedValue value(state.cx, entry && !entry->dead ? JS::ObjectValue(*entry->object) : JS::NullValue());
  JSAutoRealm realm(state.cx, object);
  if (!JS_WrapValue(state.cx, &value)) return false;
  JS::SetReservedSlot(object, kNodeShadowScopeSlot, value);
  return true;
}
bool CacheShadowNode(State& state, const NodeSnapshot& node, JS::HandleObject object) {
  NodeKey scope;
  if (!state.Check(state.model.ShadowScope(node.key, &scope))) return false;
  if (scope.node) {
    JS::RootedValue value(state.cx);
    if (!state.Wrap(scope, &value)) return false;
    JS::SetReservedSlot(object, kNodeShadowScopeSlot, value);
  }
  const NodeKey other = node.shadow_host.node ? node.shadow_host : node.shadow_root;
  if (!other.node) return true;
  JS::RootedValue value(state.cx);
  if (!state.Wrap(other, &value)) return false;
  JS::SetReservedSlot(object, kNodeShadowAssociationSlot, value);
  // The other endpoint may already have its wrapper, so Wrap's cache hit did
  // not run this hook. Store the reciprocal edge in that object's own realm.
  Wrapper* entry = state.Find(other);
  if (!entry) return TypeError(state.cx, "Shadow association lost its native endpoint");
  JS::RootedObject endpoint(state.cx, entry->object);
  JS::RootedValue back(state.cx, JS::ObjectValue(*object));
  JSAutoRealm realm(state.cx, endpoint);
  if (!JS_WrapValue(state.cx, &back)) return false;
  JS::SetReservedSlot(endpoint, kNodeShadowAssociationSlot, back);
  return true;
}
}
