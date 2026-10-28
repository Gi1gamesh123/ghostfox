#include "interaction/internal.h"
#include "js/GCVector.h"
#include "js/Wrapper.h"
#include <algorithm>

namespace ruyijs::interaction {
bool PreparePath(State& state, JS::HandleObject target, Event& event,
                 PreparedPath* result) {
  JSContext* cx = state.cx;
  auto& objects = result->objects;
  PathResult status = state.pathResolver
      ? state.pathResolver(state.pathContext, target, event.type, event.value.composed(), &objects, &result->flags)
      : PathResult::NotOwned;
  if (status == PathResult::Failure) return false;
  if (status == PathResult::NotOwned) {
    objects.clear();
    result->flags.clear();
    if (!objects.append(target)) { JS_ReportOutOfMemory(cx); return false; }
  }
  if (objects.empty() || objects[0] != target || objects.length() > 100000)
    return Error(cx, "TypeError", "Event path is inconsistent with its target registry");
  if (status == PathResult::Ready && result->flags.size() != objects.length())
    return Error(cx, "TypeError", "Event path metadata length is inconsistent");
  if (!PrepareEventProjections(state, event, result)) return false;
  result->ids.reserve(objects.length());
  result->routes.reserve(objects.length());
  for (size_t index = objects.length(); index > 0; --index) {
    JS::RootedObject object(cx, js::CheckedUnwrapStatic(objects[index - 1]));
    Target* entry = object ? state.target(object) : nullptr;
    State* owner = entry ? &state : object ? Owner(cx, object, false) : nullptr;
    if (!entry && owner) entry = owner->target(object);
    if (!entry) return Error(cx, "TypeError", "Event path contains an unregistered target");
    objects[index - 1].set(object);
    const uint64_t id = entry->aggregate.id();
    result->routes.emplace_back(id, owner);
    result->ids.push_back(id);
    if (std::find(result->states.begin(), result->states.end(), owner) == result->states.end())
      result->states.push_back(owner);
  }
  std::sort(result->routes.begin(), result->routes.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  for (size_t index = 1; index < result->routes.size(); ++index)
    if (result->routes[index - 1].first == result->routes[index].first)
      return Error(cx, "TypeError", "Event path contains a repeated target");
  if (result->states.size() == 1) result->commonOwner = result->states.front();
  return true;
}
}
