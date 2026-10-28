#ifndef RUYIJS_WEB_BROADCAST_INTERNAL_H
#define RUYIJS_WEB_BROADCAST_INTERNAL_H
#include "web_broadcast/realm.h"
#include "runtime/web_kit.h"
#include "js/StructuredClone.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ruyijs::web_broadcast {
enum ChannelSlot { ChannelName = 0, ChannelId = 1, ChannelMessage = 2, ChannelMessageError = 3,
                   ChannelSlotCount = 4 };
extern const JSClass kChannelClass;
// One open channel: closing removes it, so the registry only holds live ones.
struct Entry {
  Entry(JSContext* cx, uint64_t id, State* owner, std::string origin, std::u16string name,
        JSObject* channel)
      : id(id), owner(owner), origin(std::move(origin)), name(std::move(name)),
        channel(cx, channel) {}
  uint64_t id;
  State* owner;
  std::string origin;
  std::u16string name;
  JS::PersistentRootedObject channel;
};
struct Registry {
  uint64_t next = 1;
  Realm::RelayPort relay = nullptr;  // the Context's worker processes (remote.cpp)
  void* relayContext = nullptr;
  std::vector<std::unique_ptr<Entry>> entries;
  Entry* Find(uint64_t id) {
    for (auto& entry : entries) if (entry->id == id) return entry.get();
    return nullptr;
  }
  void Remove(uint64_t id) {
    for (size_t i = 0; i < entries.size(); ++i)
      if (entries[i]->id == id) { entries.erase(entries.begin() + i); return; }
  }
};
struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events,
        browser_environment::Environment& environment)
      : cx(cx), kit(cx, page, helpers, this), events(events), environment(environment),
        face(cx) {}
  JSContext* cx;
  WebKit kit;
  interaction::Realm& events;
  browser_environment::Environment& environment;
  WebFace face;
  // Shared with every frame realm: a retired frame may outlive the main realm.
  std::shared_ptr<Registry> registry;
  Realm::TaskPort taskPort = nullptr;
  void* taskContext = nullptr;
};
// Reads one copy of `buffer` into the entry's realm and queues its delivery task.
bool QueueDelivery(Entry& entry, JSAutoStructuredCloneBuffer& buffer);
bool Construct(JSContext* cx, unsigned argc, JS::Value* vp);
bool PostMessage(JSContext* cx, unsigned argc, JS::Value* vp);
bool Close(JSContext* cx, unsigned argc, JS::Value* vp);
bool NameGet(JSContext* cx, unsigned argc, JS::Value* vp);
bool HandlerGet(JSContext* cx, unsigned argc, JS::Value* vp);
bool HandlerSet(JSContext* cx, unsigned argc, JS::Value* vp);
}
#endif
