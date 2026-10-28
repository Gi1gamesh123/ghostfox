// Per-dispatch references to original realm registries. No persistent state.
#ifndef RUYIJS_INTERACTION_DISPATCH_PATH_H
#define RUYIJS_INTERACTION_DISPATCH_PATH_H
#include "js/GCVector.h"
#include "js/AllocPolicy.h"
#include "mozilla/Vector.h"
#include "interaction/touch_dispatch.h"
#include <cstdint>
#include <utility>
#include <vector>

namespace ruyijs::interaction {
struct State;
struct Event;
struct PreparedPath {
  explicit PreparedPath(JSContext* cx) : objects(cx), relatedTargets(cx), touch(cx) {}
  // Invocation/projection arrays are target-to-outer; ids are root-to-target.
  JS::RootedObjectVector objects, relatedTargets;
  TouchDispatchData touch;
  std::vector<uint8_t> flags;
  mozilla::Vector<uint8_t, 0, js::SystemAllocPolicy> atTarget;
  mozilla::Vector<uint64_t, 0, js::SystemAllocPolicy> projectedTargets;
  size_t currentPosition = 0;
  bool clearTargets = false, hasRelated = false;
  std::vector<uint64_t> ids;
  std::vector<std::pair<uint64_t, State*>> routes;
  std::vector<State*> states;
  State* commonOwner = nullptr;
};
struct ActiveDispatch {
  Event& event;
  PreparedPath& path;
  ActiveDispatch(Event&, PreparedPath&);
  ~ActiveDispatch();
};
bool PreparePath(State&, JS::HandleObject, Event&, PreparedPath*);
bool PrepareEventProjections(State&, Event&, PreparedPath*);
void ActivateEventProjection(Event&, PreparedPath&, size_t);
bool ComposeEventPath(JSContext*, Event&, JS::HandleObject);
bool Deliver(Event&, PreparedPath&);
}
#endif
