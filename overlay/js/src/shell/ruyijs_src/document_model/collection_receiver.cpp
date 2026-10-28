// The one brand check for ordinary DOM collections and object-owned token views.
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
CollectionRecord* CollectionReceiver(JSContext* cx, JS::HandleValue value, CollectionKind kind) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  auto* record = object && JS::GetClass(object) == &kCollectionClass
      ? static_cast<CollectionRecord*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  if (record && record->dataOwner && record->kind == kind) {
    if (record->stale && !FreshCollection(*record->dataOwner, *record)) return nullptr;
    return record;
  }
  TypeError(cx, "DOM collection receiver is incompatible");
  return nullptr;
}
}
