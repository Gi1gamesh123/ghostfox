// Original DocumentModel topology queries for Interaction's prepared dispatch.
#include "document_model/state.h"
#include "interaction/realm.h"
#include "js/GCVector.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
Wrapper* NativeNode(JSObject* object) {
  auto* entry = object && JS::GetClass(object) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  return entry && entry->owner && !entry->dead ? entry : nullptr;
}
}
interaction::PathResult Realm::EventPath(JS::HandleObject target, JS::HandleString type,
    bool composed, JS::MutableHandleObjectVector path, std::vector<uint8_t>* flags) {
  using Result = interaction::PathResult;
  JSContext* cx = state_->cx;
  JS::RootedObject object(cx, js::CheckedUnwrapStatic(target));
  Wrapper* entry = NativeNode(object);
  if (!entry) return Result::NotOwned;
  State& owner = *entry->owner;
  JSAutoRealm realm(cx, owner.page);
  std::vector<NodeKey> steps;
  std::vector<uint8_t> pending;
  if (!owner.Check(owner.model.EventPath(entry->key, composed, &steps, &pending)))
    return Result::Failure;
  bool load = false;
  if (!JS_StringEqualsLiteral(cx, type, "load", &load)) return Result::Failure;
  const bool window = !steps.empty() && Same(steps.back(), owner.main) && !load;
  // Model reserved one extra flag under Protect; this byte append cannot allocate.
  if (window) pending.push_back(0);
  JS::RootedObjectVector objects(cx);
  JS::RootedValue value(cx);
  for (const auto& step : steps) {
    if (!owner.Wrap(step, &value)) return Result::Failure;
    if (!objects.append(&value.toObject())) {
      JS_ReportOutOfMemory(cx); return Result::Failure;
    }
  }
  if (window && !objects.append(owner.page)) {
    JS_ReportOutOfMemory(cx); return Result::Failure;
  }
  // Publish only after all fallible wrapper creation and vector growth succeeds.
  if (!path.appendAll(objects)) { JS_ReportOutOfMemory(cx); return Result::Failure; }
  flags->swap(pending);
  return Result::Ready;
}

bool Realm::RetargetEventTarget(JS::HandleObject original, JS::HandleObject against,
                                JS::MutableHandleObject result, bool* inShadowTree) {
  JSContext* cx = state_->cx;
  JS::RootedObject object(cx, original ? js::CheckedUnwrapStatic(original) : nullptr);
  JS::RootedObject other(cx, against ? js::CheckedUnwrapStatic(against) : nullptr);
  if ((original && !object) || (against && !other))
    return TypeError(cx, "Event target wrapper is inaccessible");
  Wrapper* entry = NativeNode(object);
  if (!entry) {
    result.set(object); *inShadowTree = false; return true;
  }
  Wrapper* counterpart = NativeNode(other);
  State& owner = *entry->owner;
  JSAutoRealm realm(cx, owner.page);
  NodeKey projected;
  bool shadow = false;
  if (!owner.Check(owner.model.Retarget(entry->key,
      counterpart ? &counterpart->owner->model : nullptr,
      counterpart ? counterpart->key : NodeKey{}, &projected, &shadow))) return false;
  JS::RootedValue value(cx);
  if (!owner.Wrap(projected, &value)) return false;
  result.set(&value.toObject());
  *inShadowTree = shadow;
  return true;
}
}
