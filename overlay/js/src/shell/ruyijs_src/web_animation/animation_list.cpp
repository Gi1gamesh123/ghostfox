// The realm's relevant-animation list and getAnimations()
// (web-animations-1 §6.6): animations that are current or in effect, in
// composite (creation) order, filtered by their effect target's scope.
#include "web_animation/animation_internal.h"
#include "js/Array.h"
#include "js/GCVector.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include <algorithm>
#include <cstring>

namespace ruyijs::web_animation {
namespace {
constexpr size_t kRelevantLimit = 4096;  // bounded: the oldest entry leaves first
}  // namespace

bool Track(State& s, JSObject* a) {
  auto& list = s.relevant.get();
  if (std::find(list.begin(), list.end(), a) != list.end()) return true;
  if (list.length() >= kRelevantLimit) list.erase(list.begin());
  if (!list.append(a)) { JS_ReportOutOfMemory(s.cx); return false; }
  return true;
}

bool AnimationsFor(State& s, JS::HandleObject root, bool subtree, JS::MutableHandleValue result) {
  JSContext* cx = s.cx;
  JS::RootedVector<JSObject*> found(cx);
  auto& list = s.relevant.get();
  for (size_t i = 0; i < list.length();) {
    JS::RootedObject a(cx, list[i]);
    const char* state = PlayState(a);
    if (!std::strcmp(state, "idle")) { list.erase(list.begin() + i); continue; }
    ++i;
    const bool relevant = std::strcmp(state, "finished") || AnimationTiming(a).activeTime;
    JSObject* effect = EffectOf(a);
    JSObject* target = effect ? JS::GetReservedSlot(effect, kETarget).toObjectOrNull() : nullptr;
    bool inside = false;
    JS::RootedObject node(cx, target);
    if (relevant && target && !s.dom.inScope(s.dom.context, root, node, subtree, &inside)) return false;
    if (inside && !found.append(a)) return false;
  }
  std::sort(found.begin(), found.end(), [](JSObject* x, JSObject* y) {  // no GC while sorting
    return JS::GetReservedSlot(x, kASequence).toInt32() < JS::GetReservedSlot(y, kASequence).toInt32();
  });
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  if (!array) return false;
  for (size_t i = 0; i < found.length(); ++i) {
    JS::RootedValue v(cx, JS::ObjectValue(*found[i]));
    if (!JS_WrapValue(cx, &v) || !JS_DefineElement(cx, array, uint32_t(i), v, JSPROP_ENUMERATE)) return false;
  }
  result.setObject(*array);
  return true;
}
}  // namespace ruyijs::web_animation
