// Document retirement. When a top-level navigation replaces a document, or an
// iframe leaves the tree, Firefox stops running that document's timers,
// animation frames, idle callbacks and queued tasks. The one Scheduling owner
// does the same for the retired realm's global; the clock, the queues and every
// other realm's work are untouched.
#include "execution_scheduling/state.h"
#include <algorithm>
#include <utility>
#include <vector>
#include "js/GlobalObject.h"
#include "js/Wrapper.h"

namespace ruyijs::execution_scheduling {
JSObject* GlobalOf(JSObject* object) {
  if (!object) return nullptr;
  return JS::GetNonCCWObjectGlobal(js::UncheckedUnwrap(object));
}
bool State::Retired(JSObject* global) const {
  return global && std::find(retired.begin(), retired.end(), global) != retired.end();
}
void State::Drop(uint64_t id, Kind kind) {
  switch (kind) {
    case Kind::Frame: model.Cancel(id, true); break;
    case Kind::Idle: model.CancelIdle(id); break;
    case Kind::WebTask: model.CancelWebTask(id); break;
    default: model.Cancel(id, false); break;
  }
  if (!model.Exists(id)) Remove(id);
}
void State::Retire(JSObject* global) {
  if (!global || Retired(global)) return;
  retired.push_back(global);
  std::vector<std::pair<uint64_t, Kind>> doomed;
  for (Callback* cb = callbacks.get(); cb; cb = cb->next.get())
    if (cb->global == global) doomed.emplace_back(cb->id, cb->kind);
  // A callback that is running right now (the navigation task itself) is
  // purged from the model here and removed by Invoke once it returns.
  for (const auto& [id, kind] : doomed) Drop(id, kind);
}
void Realm::Retire(JS::HandleObject global) { state_->Retire(global.get()); }
bool Realm::Retired(JS::HandleObject global) const { return state_->Retired(global.get()); }
}
