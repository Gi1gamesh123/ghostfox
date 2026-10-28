// Prepare the original dispatcher's projections before invoking page callbacks.
#include "interaction/internal.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::interaction {
namespace {
bool Project(State& state, JS::HandleObject original, JS::HandleObject against,
             JS::MutableHandleObject result, bool* inShadow) {
  *inShadow = false;
  if (state.retargetResolver)
    return state.retargetResolver(state.pathContext, original, against, result, inShadow);
  result.set(original ? js::CheckedUnwrapStatic(original) : nullptr);
  return true;
}
bool TargetId(State& state, JS::HandleObject object, uint64_t* id) {
  JS::RootedObject native(state.cx, object ? js::CheckedUnwrapStatic(object) : nullptr);
  State* owner = native ? Owner(state.cx, native, false) : nullptr;
  Target* target = owner ? owner->target(native) : nullptr;
  if (!target) return Error(state.cx, "TypeError", "Projected event target is unregistered");
  *id = target->aggregate.id();
  return true;
}
}

bool PrepareEventProjections(State& state, Event& event, PreparedPath* path) {
  JSContext* cx = state.cx;
  JS::RootedObject original(cx, js::CheckedUnwrapStatic(path->objects[0]));
  JS::RootedObject related(cx);
  path->hasRelated = IsA(event.kind, Iface::Mouse) || IsA(event.kind, Iface::Focus);
  if (path->hasRelated) {
    const auto& value = JS::GetReservedSlot(event.object, kEventRelatedTarget);
    if (value.isObject()) related = js::CheckedUnwrapStatic(&value.toObject());
  }
  if (!PrepareTouchDispatch(cx, event, path->touch)) return false;
  const auto count = path->touch.members.length();
  if (count && path->objects.length() > 1000000 / count)
    return Error(cx, "RangeError", "Event target projection limit exceeded");
  size_t retained = 0;
  for (size_t index = 0; index < path->objects.length(); ++index) {
    JS::RootedObject against(cx, js::CheckedUnwrapStatic(path->objects[index]));
    JS::RootedObject target(cx), adjustedRelated(cx);
    bool targetShadow = false, relatedShadow = false;
    if (!Project(state, original, against, &target, &targetShadow) ||
        !Project(state, related, against, &adjustedRelated, &relatedShadow)) return false;
    // An originally self-related target propagates within its own tree. Equality
    // caused by crossing a shadow boundary, or retargeting another node, trims.
    if (adjustedRelated && target == adjustedRelated &&
        (original != related || target != original)) break;
    uint64_t id = 0;
    if (!TargetId(state, target, &id)) return false;
    if (!path->projectedTargets.append(id) || !path->atTarget.append(target == against ? 1 : 0)) {
      JS_ReportOutOfMemory(cx); return false;
    }
    {
      JSAutoRealm realm(cx, event.object);
      if (!JS_WrapObject(cx, &adjustedRelated) || !path->relatedTargets.append(adjustedRelated)) {
        if (!JS_IsExceptionPending(cx)) JS_ReportOutOfMemory(cx);
        return false;
      }
    }
    bool clear = targetShadow || relatedShadow;
    for (size_t member = 0; member < count; ++member) {
      JS::RootedObject source(cx, path->touch.originalTargets[member]), adjusted(cx);
      bool shadow = false;
      if (!Project(state, source, against, &adjusted, &shadow)) return false;
      if (!path->touch.targets.append(adjusted)) { JS_ReportOutOfMemory(cx); return false; }
    }
    path->clearTargets = clear;
    ++retained;
  }
  if (!retained) {
    // Firefox computes clearing from the unhandled original targets when the
    // first position was trimmed, before publishing any related projection.
    JS::RootedObject unused(cx);
    bool targetShadow = false, relatedShadow = false;
    if (!Project(state, original, original, &unused, &targetShadow) ||
        !Project(state, related, related, &unused, &relatedShadow)) return false;
    path->clearTargets = targetShadow || relatedShadow;
  }
  path->objects.get().shrinkTo(retained);
  if (!path->flags.empty()) path->flags.resize(retained);
  return CompleteTouchDispatch(cx, path->touch, retained);
}

void ActivateEventProjection(Event& event, PreparedPath& path, size_t position) {
  path.currentPosition = position;
  event.value.set_target_id(path.projectedTargets[position]);
  if (path.hasRelated)
    JS::SetReservedSlot(event.object, kEventRelatedTarget,
                        JS::ObjectOrNullValue(path.relatedTargets[position]));
  ApplyTouchDispatch(event, path.touch, position);
}
}
