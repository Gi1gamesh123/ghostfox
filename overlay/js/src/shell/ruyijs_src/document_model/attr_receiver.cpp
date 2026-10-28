// The unique Attr brand and current source-node owner, independent of callee realm.
#include "document_model/attr_object.h"
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
Wrapper* SourceOf(JSObject* object) {
  const auto source = JS::GetReservedSlot(object, 1);
  JSObject* node = source.isObject() ? js::CheckedUnwrapStatic(&source.toObject()) : nullptr;
  auto* entry = node && JS::GetClass(node) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(node, 0).toPrivate()) : nullptr;
  return entry && entry->owner && !entry->dead ? entry : nullptr;
}
}
JSObject* AttrObject(JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  return object && JS::GetClass(object) == &kAttrClass ? object : nullptr;
}
AttrSlot* AttrSlotOf(JS::HandleValue value) {
  JSObject* object = AttrObject(value);
  if (!object) return nullptr;
  auto* slot = static_cast<AttrSlot*>(JS::GetReservedSlot(object, 0).toPrivate());
  if (!slot || (!slot->element.document && !slot->element.node)) return slot;
  Wrapper* source = SourceOf(object);
  if (!source) return nullptr;
  slot->element = source->key;
  return slot;
}
State& AttrOwner(State& fallback, JS::HandleValue value) {
  JSObject* object = AttrObject(value);
  Wrapper* source = object ? SourceOf(object) : nullptr;
  return source ? *source->owner : fallback;
}
}
