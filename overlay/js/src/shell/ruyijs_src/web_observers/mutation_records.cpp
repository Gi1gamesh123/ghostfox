// Queue original node identities through the existing DOM factories.
#include "web_observers/internal.h"
#include "document_model/realm.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <utility>

namespace ruyijs::web_observers {
namespace {
const char* KindName(MutationKind kind) {
  if (kind == MutationKind::Attributes) return "attributes";
  if (kind == MutationKind::CharacterData) return "characterData";
  return "childList";
}
bool Text(JSContext* cx, const DomString& text, JS::MutableHandleValue result) {
  JS::RootedString value(cx, JS_NewUCStringCopyN(cx, text.data(), text.size()));
  if (!value) return false;
  result.setString(value);
  return true;
}
}  // namespace

bool State::Append(Observer& observer, const MutationNotice& notice, bool wants_old_value) {
  JSAutoRealm realm(cx, observer.owner->page);
  JS::Rooted<Pending> rooted(cx, Pending());
  Pending& pending = rooted.get();
  pending.kind = notice.kind;
  JS::RootedObject node(cx);
  if (!Resolve(notice.target, &node) || !node || !JS_WrapObject(cx, &node)) return false;
  pending.target = node;
  document_model::NodeSource targetSource;
  JS::RootedValue targetValue(cx, JS::ObjectValue(*node));
  if (!document->ResolveNodeSource(targetValue, &targetSource)) return false;
  if (!Resolve(notice.previous_sibling, &node) || !JS_WrapObject(cx, &node)) return false;
  pending.previous = node;
  if (!Resolve(notice.next_sibling, &node) || !JS_WrapObject(cx, &node)) return false;
  pending.next = node;
  if (!notice.added.empty()) {
    if (!document->NodeListOf(notice.added, &node, targetSource.factory) || !JS_WrapObject(cx, &node)) return false;
    pending.added = node;
  }
  if (!notice.removed.empty()) {
    if (!document->NodeListOf(notice.removed, &node, targetSource.factory) || !JS_WrapObject(cx, &node)) return false;
    pending.removed = node;
  }
  if (notice.kind == MutationKind::Attributes) {
    pending.has_attribute_name = true;
    pending.attribute_name = notice.attribute_name;
    pending.attribute_namespace = notice.attribute_namespace;
  }
  // An attribute or data value that did not exist before has no old value: the
  // notice carries none, so the record's oldValue stays null.
  if (wants_old_value && notice.has_old_value) {
    pending.has_old_value = true;
    pending.old_value = notice.old_value;
  }
  if (!observer.pending.get().append(std::move(pending))) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  return true;
}

bool State::Resolve(NodeKey key, JS::MutableHandleObject result) {
  result.set(nullptr);
  if (key.document == 0 && key.node == 0) return true;
  JS::AutoSaveExceptionState saved(cx);
  JS::RootedObject object(cx);
  if (document->WrapNode(key, &object)) { result.set(object); return true; }
  // A vanished identity is a dropped slot, not a failure of the whole delivery.
  saved.restore();
  return true;
}

bool State::RecordOf(const Pending& pending, JS::MutableHandleObject result) {
  result.set(nullptr);
  {
  JSAutoRealm realm(cx, page);
  JS::RootedObject target(cx, pending.target);
  if (!target) return true;
  JS::RootedObject object(
      cx, JS_NewObjectWithGivenProto(cx, &kRecordClass, recordPrototype));
  JS::RootedString type(cx, JS_NewStringCopyZ(cx, KindName(pending.kind)));
  JS::RootedObject added(cx, pending.added), removed(cx, pending.removed);
  JS::RootedObject previous(cx, pending.previous), next(cx, pending.next);
  if (!object || !type) return false;
  // Empty lists have no identity to retain. Allocate them only when a record
  // is consumed, keeping discarded attribute batches compact.
  if (!added || !removed) {
    document_model::NodeSource source;
    JS::RootedValue value(cx, JS::ObjectValue(*target));
    if (!document->ResolveNodeSource(value, &source) ||
        (!added && !source.owner->NodeListOf({}, &added, source.factory)) ||
        (!removed && !source.owner->NodeListOf({}, &removed, source.factory))) return false;
  }
  if (!JS_WrapObject(cx, &target) || !JS_WrapObject(cx, &added) ||
      !JS_WrapObject(cx, &removed) || !JS_WrapObject(cx, &previous) ||
      !JS_WrapObject(cx, &next)) return false;
  JS::RootedValue attribute_name(cx, JS::NullValue());
  JS::RootedValue attribute_namespace(cx, JS::NullValue());
  JS::RootedValue old_value(cx, JS::NullValue());
  if (pending.has_attribute_name && !Text(cx, pending.attribute_name, &attribute_name))
    return false;
  if (pending.has_attribute_name && !pending.attribute_namespace.empty() &&
      !Text(cx, pending.attribute_namespace, &attribute_namespace)) return false;
  if (pending.has_old_value && !Text(cx, pending.old_value, &old_value)) return false;
  JS::SetReservedSlot(object, 0, JS::StringValue(type));
  JS::SetReservedSlot(object, 1, JS::ObjectValue(*target));
  JS::SetReservedSlot(object, 2, JS::ObjectValue(*added));
  JS::SetReservedSlot(object, 3, JS::ObjectValue(*removed));
  JS::SetReservedSlot(object, 4, previous ? JS::ObjectValue(*previous) : JS::NullValue());
  JS::SetReservedSlot(object, 5, next ? JS::ObjectValue(*next) : JS::NullValue());
  JS::SetReservedSlot(object, 6, attribute_name);
  JS::SetReservedSlot(object, 7, attribute_namespace);
  JS::SetReservedSlot(object, 8, old_value);
  result.set(object);
  }
  return JS_WrapObject(cx, result);
}

bool State::BuildBatch(const PendingRecords& batch, JS::MutableHandleObject result) {
  JS::RootedObject array(cx, JS::NewArrayObject(cx, batch.length()));
  if (!array) return false;
  uint32_t index = 0;
  for (const Pending& pending : batch) {
    JS::RootedObject record(cx);
    if (!RecordOf(pending, &record)) return false;
    if (!record) continue;
    JS::RootedValue item(cx, JS::ObjectValue(*record));
    if (!JS_DefineElement(cx, array, index++, item, JSPROP_ENUMERATE)) return false;
  }
  JS::RootedValue length(cx, JS::NumberValue(index));
  if (!JS_SetProperty(cx, array, "length", length)) return false;
  result.set(array);
  return true;
}

}  // namespace ruyijs::web_observers
