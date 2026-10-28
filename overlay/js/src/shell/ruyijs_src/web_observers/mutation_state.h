#ifndef RUYIJS_WEB_OBSERVERS_MUTATION_STATE_H
#define RUYIJS_WEB_OBSERVERS_MUTATION_STATE_H
#include <memory>
#include <vector>
#include "document_model/model.h"
#include "js/GCVector.h"

namespace ruyijs::web_observers {
struct State;

// Local keys are interpreted only by their original source owner.
struct Registration {
  State* source = nullptr;
  document_model::NodeKey target;
  bool child_list = false, attributes = false, character_data = false;
  bool subtree = false, attribute_old_value = false, character_data_old_value = false;
  bool all_attributes = true;
  std::vector<document_model::DomString> filter;
  uint64_t baseline = 0;
  // DOM "transient registered observer": `origin` is the id of the registration
  // it was copied from (0 for an observe() registration). It lives on a node
  // removed from that registration's subtree until the observer's next
  // delivery or a new observe() of the origin target.
  uint64_t id = 0, origin = 0;
};

// Freeze matching decisions and original identities at notification time.
// The lists come from the DocumentModel factory; no mutable keys remain here.
struct Pending {
  document_model::MutationKind kind = document_model::MutationKind::ChildList;
  JS::Heap<JSObject*> target, previous, next, added, removed;
  document_model::DomString attribute_name, attribute_namespace, old_value;
  bool has_attribute_name = false, has_old_value = false;
  void trace(JSTracer* tracer) {
    JS::TraceEdge(tracer, &target, "MutationObserver target");
    JS::TraceEdge(tracer, &previous, "MutationObserver previous sibling");
    JS::TraceEdge(tracer, &next, "MutationObserver next sibling");
    JS::TraceEdge(tracer, &added, "MutationObserver added nodes");
    JS::TraceEdge(tracer, &removed, "MutationObserver removed nodes");
  }
};
using PendingRecords = JS::GCVector<Pending, 0, js::SystemAllocPolicy>;

struct Observer {
  State* owner = nullptr;
  // Existing observer/callback lifetime is unchanged; targets are never roots.
  JS::PersistentRootedObject object, callback;
  std::vector<Registration> registrations;
  uint64_t nextRegistration = 0;
  JS::PersistentRooted<PendingRecords> pending;
  bool scheduled = false;
  Observer* previousInGroup = nullptr;
  Observer* nextInGroup = nullptr;
  std::unique_ptr<Observer> next;
  Observer(JSContext* cx, JS::HandleObject value, JS::HandleObject callable)
      : object(cx, value), callback(cx, callable),
        pending(cx, PendingRecords()) {}
};
}  // namespace ruyijs::web_observers
#endif
