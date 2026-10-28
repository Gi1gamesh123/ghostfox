#ifndef RUYIJS_WEB_SERVICE_WORKER_INTERNAL_H
#define RUYIJS_WEB_SERVICE_WORKER_INTERNAL_H
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "web_service_worker/realm.h"
#include "web_service_worker/registry.h"
#include "runtime/secure_slot.h"
#include "runtime/web_kit.h"

namespace ruyijs::web_workers { class Realm; }
namespace ruyijs::web_service_worker {
using TaskPort = bool (*)(void*, JS::HandleObject);
enum Slot { SlotControllerChange = 0, SlotMessage, SlotMessageError, SlotCount };
extern const JSClass kContainerClass;
extern const char* const kHandlers[SlotCount];

struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events,
        browser_environment::Environment& environment)
      : cx(cx), kit(cx, page, helpers, this), events(events), environment(environment), face(cx),
        instance(cx), ready(cx), exception(cx), secure(cx) {}
  JSContext* cx;
  WebKit kit;
  interaction::Realm& events;
  browser_environment::Environment& environment;
  WebFace face;
  JS::PersistentRootedObject instance, ready, exception;
  SecureSlots secure;
  // ServiceWorker, ServiceWorkerRegistration, NavigationPreloadManager prototypes.
  JS::PersistentRootedObject prototypes[3] = {JS::PersistentRootedObject(cx), JS::PersistentRootedObject(cx),
                                              JS::PersistentRootedObject(cx)};
  // Execution (Window realms with a Web Workers owner; null elsewhere).
  web_workers::Realm* workers = nullptr;
  std::shared_ptr<Registry> registry;
  std::string clientId;
  bool nested = false;
  TaskPort task = nullptr;
  void* taskContext = nullptr;
  bool readyResolved = false;
  uint64_t controller = 0;  // registration id controlling this client
  bool messagesEnabled = false;
  bool (*parsed)(void*) = nullptr;
  void* parsedContext = nullptr;
  std::deque<std::pair<uint64_t, std::string>> pendingMessages;  // version id, clone frame
  // SameObject registration / worker objects by record id.
  std::map<uint64_t, std::unique_ptr<JS::PersistentRootedObject>> objects;
  std::vector<std::pair<uint64_t, std::unique_ptr<JS::PersistentRootedObject>>> pendingRegisters;
  struct ScopeState* scope = nullptr;  // ServiceWorkerGlobalScope realm (scope.cpp)
};
enum ObjectKind { kWorkerObject = 0, kRegistrationObject = 1, kPreloadObject = 2 };
// A new instance of one of the three interfaces (registration.cpp).
JSObject* NewObject(State&, ObjectKind);
// Methods of those instances once a registry exists (calls.cpp); `member` is the WebIDL name.
bool CallMember(State&, ObjectKind, const char* member, JS::HandleObject self, const JS::CallArgs&);
bool Reject(State&, JS::MutableHandleValue rval);
bool RejectDom(State&, const std::string& message, const char* name, JS::MutableHandleValue rval);
bool ThrowDom(State&, const std::string& message, const char* name);
bool PostTask(State&, JSNative native, uint64_t id, const char* name);

inline std::string Incompatible(const char* member, const char* iface) {
  return std::string("'") + member + "' called on an object that does not implement interface " + iface + ".";
}
// The container behind |this|, or a pending receiver TypeError naming |member|.
JSObject* Container(JSContext* cx, const JS::CallArgs& args, const char* member);
// Attribute and plain members (members.cpp).
bool StartMessages(JSContext*, unsigned, JS::Value*);
bool Controller(JSContext*, unsigned, JS::Value*);
bool Ready(JSContext*, unsigned, JS::Value*);
bool HandlerGet(JSContext*, unsigned, JS::Value*);
bool HandlerSet(JSContext*, unsigned, JS::Value*);
// ServiceWorker, ServiceWorkerRegistration, NavigationPreloadManager (registration.cpp).
bool InstallRegistration(State&);
// PushManager, PushSubscription, PushSubscriptionOptions (push.cpp).
bool InstallPush(State&);
// CookieStoreManager, interface object only (cookie_manager.cpp).
bool InstallCookieManager(State&);
}
#endif
