// BroadcastChannel across processes (docs/owners/BROADCAST_CHANNEL.md): a message posted
// in this registry is also handed to its relay port (the runtime encodes it for the
// Context's worker processes), and a message relayed from another process is
// delivered here to every open channel of that origin and name, each in its own task.
#include "web_broadcast/internal.h"
#include "jsapi.h"
#include "js/StructuredClone.h"

namespace ruyijs::web_broadcast {
namespace {
void CloneError(JSContext*, uint32_t, void* closure, const char*) { *static_cast<bool*>(closure) = true; }
const JSStructuredCloneCallbacks kCallbacks = {nullptr, nullptr, CloneError};
}  // namespace

void Realm::SetRelayPort(RelayPort port, void* context) {
  state_->registry->relay = port;
  state_->registry->relayContext = context;
}
void Realm::ClearRelayPort(void* context) {
  if (state_->registry->relayContext != context) return;
  state_->registry->relay = nullptr;
  state_->registry->relayContext = nullptr;
}
bool Realm::Receive(const std::string& origin, const std::u16string& name, JS::HandleValue data) {
  State& s = *state_;
  JSContext* cx = s.cx;
  bool cloneError = false;
  JSAutoStructuredCloneBuffer buffer(JS::StructuredCloneScope::SameProcess, &kCallbacks, &cloneError);
  if (!buffer.write(cx, data, &kCallbacks, &cloneError)) return cloneError ? true : false;
  std::vector<uint64_t> targets;
  for (auto& entry : s.registry->entries)
    if (entry->origin == origin && entry->name == name) targets.push_back(entry->id);
  for (uint64_t id : targets)
    if (Entry* entry = s.registry->Find(id); entry && !QueueDelivery(*entry, buffer)) return false;
  return true;
}
}  // namespace ruyijs::web_broadcast
