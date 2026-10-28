// One domain dispatch algorithm, routed through the path's original owners.
#include "interaction/internal.h"
#include "js/Object.h"
#include <algorithm>

namespace ruyijs::interaction {
namespace {
State* DispatchOwner(const PreparedPath& path, uint64_t id) {
  if (path.commonOwner) return path.commonOwner;
  auto route = std::lower_bound(path.routes.begin(), path.routes.end(), id,
      [](const auto& entry, uint64_t key) { return entry.first < key; });
  return route == path.routes.end() || route->first != id ? nullptr : route->second;
}
}
ActiveDispatch::ActiveDispatch(Event& entry, PreparedPath& prepared)
    : event(entry), path(prepared) { event.dispatchPath = &path; }
ActiveDispatch::~ActiveDispatch() {
  if (!path.objects.empty()) ActivateEventProjection(event, path, path.objects.length() - 1);
  if (path.clearTargets) {
    event.value.set_target_id(0);
    if (path.hasRelated)
      JS::SetReservedSlot(event.object, kEventRelatedTarget, JS::NullValue());
  }
  event.dispatchPath = nullptr;
  if (event.value.dispatching()) event.value.end_dispatch();
  for (State* owner : path.states) owner->cleanup();
}
bool Deliver(Event& event, PreparedPath& path) {
  ActiveDispatch active(event, path);
  return domain::EventDispatchService::dispatch_resolved(event.value, path.ids,
      [&](uint64_t id) -> domain::EventTargetAggregate* {
        State* owner = DispatchOwner(path, id);
        if (!owner) return nullptr;
        auto target = owner->targetMap.find(id);
        return target == owner->targetMap.end() ? nullptr : target->second;
      },
      [&](domain::ListenerId id, uint64_t target) -> domain::ListenerCallback {
        State* owner = DispatchOwner(path, target);
        if (!owner) return {};
        auto callback = owner->callbacks.find(id);
        return callback == owner->callbacks.end() ? domain::ListenerCallback() : callback->second;
      },
      [&](size_t index) { return path.atTarget[path.ids.size() - 1 - index] != 0; },
      [&](size_t index) { ActivateEventProjection(event, path, path.ids.size() - 1 - index); });
}
}
