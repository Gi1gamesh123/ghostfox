// BroadcastChannel transport between the processes of one Context (docs/owners/
// BROADCAST_CHANNEL.md): every web_workers realm sharing a Budget forms one group;
// a broadcast frame ('\x07') goes to each live worker process of the group except
// the one it came from. The payload is the runtime's; this owner only routes it.
#include "web_workers/realm.h"
#include "web_workers/state.h"
#include "js/Object.h"
#include <algorithm>

namespace ruyijs::web_workers {
namespace {
WorkerRecord* Record(JSObject* object) {
  return static_cast<WorkerRecord*>(JS::GetReservedSlot(object, 0).toPrivate());
}
}  // namespace

void Realm::SetBroadcastSink(BroadcastSink sink, void* context) {
  state_->broadcastSink = sink;
  state_->broadcastContext = context;
}
bool Realm::Broadcast(const std::string& frame, const void* except) {
  for (State* state : state_->budget->group) {
    for (JSObject* object : state->active) {
      WorkerRecord* record = Record(object);
      if (record != except && !record->terminated && record->process.Running()) record->process.Queue(frame);
    }
    for (auto& entry : state->services)
      if (&entry->record != except && !entry->record.terminated) entry->record.process.Queue(frame);
  }
  return true;
}
bool Realm::HasBroadcastTargets() const {
  for (State* state : state_->budget->group)
    if (!state->active.empty() || !state->services.empty()) return true;
  return false;
}
bool RouteBroadcast(State& state, std::string_view frame, const void* from) {
  return !state.broadcastSink || state.broadcastSink(state.broadcastContext, frame, from);
}
void JoinGroup(State& state) { state.budget->group.push_back(&state); }
void LeaveGroup(State& state) { std::erase(state.budget->group, &state); }
}  // namespace ruyijs::web_workers
