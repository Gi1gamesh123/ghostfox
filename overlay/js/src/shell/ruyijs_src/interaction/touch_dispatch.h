#pragma once
#include "js/GCVector.h"
#include "js/RootingAPI.h"
#include <cstddef>

namespace ruyijs::interaction {
struct Event;
// Original PreparedPath stack only; no second owner registry or dispatcher.
struct TouchDispatchData {
  explicit TouchDispatchData(JSContext* cx)
      : members(cx), originalTargets(cx), targets(cx), emptyBackings(cx),
        initialMembers(cx), initialTargets(cx), initialBacking(cx), list(cx) {}
  JS::RootedObjectVector members, originalTargets;
  // Generic preparation fills target->outer position-major projections.
  JS::RootedObjectVector targets, emptyBackings, initialMembers, initialTargets;
  JS::RootedObject initialBacking, list;
  bool active = false, initialPresent = false, started = false;
  size_t lastOffset = 0, nextEmpty = 0;
};
bool PrepareTouchDispatch(JSContext*, Event&, TouchDispatchData&);
bool CompleteTouchDispatch(JSContext*, TouchDispatchData&, size_t positions);
// Targets are already wrapped into the corresponding Touch realm. No allocation.
void ApplyTouchDispatch(Event&, TouchDispatchData&, size_t position);
}
