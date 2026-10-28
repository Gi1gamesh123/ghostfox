#ifndef RUYIJS_WEB_WAKE_LOCK_INTERNAL_H
#define RUYIJS_WEB_WAKE_LOCK_INTERNAL_H
#include <string>
#include "web_wake_lock/realm.h"
#include "runtime/secure_slot.h"
#include "runtime/web_kit.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment { class Environment; }

namespace ruyijs::web_wake_lock {
enum SentinelSlot { SentinelReleased = 0, SentinelHandler, SentinelSlotCount };
extern const JSClass kWakeLockClass, kSentinelClass;

struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events,
        browser_environment::Environment& environment)
      : cx(cx), kit(cx, page, helpers, this), events(events), environment(environment),
        wakeLock(cx), sentinel(cx), instance(cx), exception(cx), secure(cx) {}
  JSContext* cx;
  WebKit kit;
  interaction::Realm& events;
  browser_environment::Environment& environment;
  WebFace wakeLock, sentinel;
  JS::PersistentRootedObject instance, exception;
  SecureSlots secure;
  Realm::TaskPort taskPort = nullptr;
  void* taskContext = nullptr;
};

inline std::string Incompatible(const char* member, const char* iface) {
  return std::string("'") + member + "' called on an object that does not implement interface " + iface + ".";
}
// A promise-returning operation reports a pending exception as a rejection.
bool RejectPending(JSContext* cx, JS::MutableHandleValue rval);
// WakeLockSentinel members (sentinel.cpp).
bool Release(JSContext*, unsigned, JS::Value*);
bool Released(JSContext*, unsigned, JS::Value*);
bool Type(JSContext*, unsigned, JS::Value*);
bool HandlerGet(JSContext*, unsigned, JS::Value*);
bool HandlerSet(JSContext*, unsigned, JS::Value*);
}
#endif
