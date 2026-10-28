// The one production installer of LockManager and Lock (dom/webidl/LockManager.webidl,
// Lock.webidl) and of the SameObject navigator.locks getter.
#include "web_locks/internal.h"
#include "browser_environment/environment.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_locks {
const JSClass kManagerClass = {"LockManager", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kLockClass = {"Lock", JSCLASS_HAS_RESERVED_SLOTS(LockSlotCount)};
namespace {
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
bool NavigatorLocks(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedValue navigator(cx);
  JSObject* self = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  {
    JSAutoRealm realm(cx, s.kit.page);
    if (!JS_GetProperty(cx, s.kit.page, "navigator", &navigator)) return false;
  }
  if (!self || !navigator.isObject() ||
      JS::GetClass(self) != JS::GetClass(js::UncheckedUnwrap(&navigator.toObject())))
    return WebKit::Fail(cx, "'get locks' called on an object that does not implement interface Navigator.");
  JS::RootedValue value(cx, JS::ObjectValue(*s.instance));
  if (!JS_WrapValue(cx, &value)) return false;
  args.rval().set(value);
  return true;
}
}  // namespace

State::~State() {
  if (!registry) return;
  for (auto it = registry->owners.begin(); it != registry->owners.end();)
    it = it->second == this ? registry->owners.erase(it) : std::next(it);
  // A realm going away releases what it held and drops what it waited for.
  for (auto& [origin, queue] : registry->queues) {
    const auto granted = queue.RemoveClient(client);
    if (!granted.empty()) (void)DeliverGranted(*registry, granted);
  }
}

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             browser_environment::Environment& environment, Realm* parent)
    : state_(std::make_unique<State>(cx, page, helpers, environment)) {
  state_->registry = parent ? parent->state_->registry : std::make_shared<Registry>();
  state_->client = std::to_string(reinterpret_cast<uintptr_t>(state_.get()));
}
Realm::~Realm() = default;
void Realm::SetTaskPort(TaskPort port, void* context) {
  state_->taskPort = port;
  state_->taskContext = context;
}

bool Realm::Install() {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedValue exception(cx);
  if (!JS_GetProperty(cx, s.kit.page, "DOMException", &exception) || !exception.isObject())
    return WebKit::Fail(cx, "Web Locks requires the realm's DOMException");
  s.exception = &exception.toObject();
  s.requests = JS::NewArrayObject(cx, 0);
  JS::RootedObject objectPrototype(cx, JS::GetRealmObjectPrototype(cx));
  if (!s.requests || !objectPrototype ||
      !s.kit.Define("LockManager", &kManagerClass, objectPrototype, Illegal, &s.manager) ||
      !s.kit.Method(s.manager, "LockManager", "request", Request, 2, 0) ||
      !s.kit.Method(s.manager, "LockManager", "query", Query, 0, 0) ||
      !s.kit.Define("Lock", &kLockClass, objectPrototype, Illegal, &s.lock) ||
      !s.kit.Accessor(s.lock, "Lock", "name", LockName_, nullptr, 0) ||
      !s.kit.Accessor(s.lock, "Lock", "mode", LockMode_, nullptr, 0)) return false;
  s.instance = s.kit.New(&kManagerClass, s.manager.prototype);
  if (!s.instance) return false;
  JS::SetReservedSlot(s.instance, 0, JS::PrivateValue(&s));
  JS::RootedObject navigatorPrototype(cx, s.environment.navigatorPrototype());
  JS::RootedObject getter(cx, s.kit.Function(NavigatorLocks, "get locks", 0, 0)), setter(cx);
  return navigatorPrototype && getter &&
         JS_DefineProperty(cx, navigatorPrototype, "locks", getter, setter, JSPROP_ENUMERATE) &&
         s.kit.helpers->Accessor("Navigator", navigatorPrototype, "locks") &&
         s.kit.helpers->Object("LockManager", s.instance) &&
         s.secure.Add(navigatorPrototype, "locks") && s.secure.Add(s.kit.page, "LockManager") &&
         s.secure.Add(s.kit.page, "Lock");
}

bool Realm::ConfigureExposure() {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedValue secure(cx);
  return JS_GetProperty(cx, s.kit.page, "isSecureContext", &secure) && s.secure.Apply(secure.isTrue());
}
}  // namespace ruyijs::web_locks
