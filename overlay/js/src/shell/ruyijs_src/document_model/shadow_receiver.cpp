#include "document_model/state.h"

namespace ruyijs::document_model {
Wrapper* ShadowReceiver(JSContext* cx, JS::HandleValue value) {
  Wrapper* entry = NodeReceiver(cx, value);
  if (!entry) return nullptr;
  NodeSnapshot node;
  if (!entry->owner->Check(entry->owner->model.Inspect(entry->key, &node))) return nullptr;
  if (node.shadow_host.node) return entry;
  TypeError(cx, "ShadowRoot receiver is incompatible");
  return nullptr;
}
}
