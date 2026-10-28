#ifndef RUYIJS_WEB_LOCKS_INTERNAL_H
#define RUYIJS_WEB_LOCKS_INTERNAL_H
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "domain/locks/lock_queue.h"
#include "runtime/secure_slot.h"
#include "runtime/web_kit.h"
#include "web_locks/realm.h"

namespace ruyijs::web_locks {
namespace locks = ::ruyijs::domain::locks;
// One origin's queue plus the realms whose requests it holds, so a grant is
// delivered to the realm that made the request.
struct Registry {
  std::map<std::string, locks::LockQueue> queues;
  std::map<uint64_t, State*> owners;
  uint64_t next = 1;
};
// A request this realm made: its promise capability, callback and the Lock
// object once granted. Rooted in the realm's request table.
enum RequestSlot { ReqPromise = 0, ReqCallback, ReqLock, ReqResolve, ReqReject, ReqSlotCount };
enum LockSlot { LockName = 0, LockMode = 1, LockSlotCount = 2 };
extern const JSClass kManagerClass, kLockClass;

struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        browser_environment::Environment& environment)
      : cx(cx), kit(cx, page, helpers, this), environment(environment), manager(cx), lock(cx),
        instance(cx), exception(cx), secure(cx), requests(cx) {}
  ~State();
  JSContext* cx;
  WebKit kit;
  browser_environment::Environment& environment;
  WebFace manager, lock;
  JS::PersistentRootedObject instance, exception;
  // [SecureContext]: navigator.locks, LockManager, Lock.
  SecureSlots secure;
  // id -> request record (an Array of RequestSlot entries).
  JS::PersistentRooted<JSObject*> requests;
  std::shared_ptr<Registry> registry;
  std::string client;
  Realm::TaskPort taskPort = nullptr;
  void* taskContext = nullptr;
};

bool Request(JSContext* cx, unsigned argc, JS::Value* vp);
bool Query(JSContext* cx, unsigned argc, JS::Value* vp);
bool LockName_(JSContext* cx, unsigned argc, JS::Value* vp);
bool LockMode_(JSContext* cx, unsigned argc, JS::Value* vp);
// The realm origin used as the queue key (opaque origins get their own key).
bool Origin(State& state, std::string* origin);
// Delivers grants and steals produced by one queue step to their realms.
bool Deliver(Registry& registry, const locks::LockQueue::Step& step);
bool DeliverGranted(Registry& registry, const std::vector<locks::LockRequest>& granted);
// A DOMException of this realm, as a value.
bool Exception(State& state, const char* name, const char* message, JS::MutableHandleValue out);
}
#endif
