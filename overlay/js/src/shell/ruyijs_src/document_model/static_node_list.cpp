// The original object-owned collection factory for query and mutation snapshots.
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Realm.h"
#include <new>

namespace ruyijs::document_model {
bool State::NodeListOwned(const std::vector<NodeKey>& items, JS::MutableHandleObject result,
                          CollectionKind kind, State* factory) {
  State& allocation = factory ? *factory : *this;
  JSAutoRealm realm(cx, allocation.page);
  Interface* installed = allocation.Find(kind == CollectionKind::HTMLCollection ? "HTMLCollection" : "NodeList");
  if (!installed) return TypeError(cx, "Collection interface is not installed");
  JS::RootedObject prototype(cx, installed->prototype);
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kCollectionClass, prototype));
  if (!object) return false;
  auto* record = new (std::nothrow) CollectionRecord();
  if (!record) { JS_ReportOutOfMemory(cx); return false; }
  record->kind = kind;
  record->live = false;
  record->dataOwner = this;
  JS::SetReservedSlot(object, 0, JS::PrivateValue(record));
  JS::SetReservedSlot(object, 1, JS::BooleanValue(true));
  // Attach before any GC-capable lookup: the rooted list now traces every
  // captured node and its finalizer owns cleanup on all subsequent failures.
  record->objects.reserve(items.size());
  for (NodeKey key : items) {
    JS::RootedValue value(cx);
    if (!Wrap(key, &value)) return false;
    // Preserve the previous factory's treatment of already expired log keys.
    // Once captured, a node is never resolved from a mutable Model key again.
    if (value.isObject()) record->objects.emplace_back(&value.toObject());
  }
  if (!MaterializeCollection(*this, object, *record)) return false;
  result.set(object);
  return true;
}
}  // namespace ruyijs::document_model
