// LockManager.request/query and the grant delivery, following donor
// dom/locks/LockManager.cpp (argument validation), LockManagerParent.cpp (the
// queue, via domain/locks/lock_queue.h) and LockRequestChild.cpp /
// Lock.cpp (callback, waiting promise and release).
#include "web_locks/internal.h"
#include "browser_environment/environment.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Array.h"
#include "js/Realm.h"

namespace ruyijs::web_locks {
namespace {
State* Manager(JSContext* cx, const JS::CallArgs& args, const char* member) {
  JS::RootedObject self(cx);
  std::string message = std::string("'") + member +
      "' called on an object that does not implement interface LockManager.";
  if (!WebKit::Check(cx, args.thisv(), &kManagerClass, &self, message.c_str())) return nullptr;
  return static_cast<State*>(JS::GetReservedSlot(self, 0).toPrivate());
}
// A promise-returning operation reports its failure as a rejected promise.
bool RejectPending(JSContext* cx, JS::MutableHandleValue rval) {
  JS::RootedValue error(cx);
  if (!JS_GetPendingException(cx, &error)) return false;
  JS_ClearPendingException(cx);
  JSObject* promise = JS::CallOriginalPromiseReject(cx, error);
  if (!promise) return false;
  rval.setObject(*promise);
  return true;
}
bool RejectWith(State& s, const char* name, const char* message, JS::MutableHandleValue rval) {
  JS::RootedValue error(s.cx);
  if (!Exception(s, name, message, &error)) return false;
  JSObject* promise = JS::CallOriginalPromiseReject(s.cx, error);
  if (!promise) return false;
  rval.setObject(*promise);
  return true;
}
bool Record(State& s, uint64_t id, JS::MutableHandleObject record) {
  JS::RootedValue value(s.cx);
  if (!JS_GetProperty(s.cx, s.requests, std::to_string(id).c_str(), &value)) return false;
  record.set(value.isObject() ? &value.toObject() : nullptr);
  return true;
}
bool Forget(State& s, uint64_t id) {
  return JS_DeleteProperty(s.cx, s.requests, std::to_string(id).c_str());
}
// Lock released: the waiting promise settled. Release, grant waiters, and
// settle the request promise with the same outcome.
bool Released(JSContext* cx, unsigned argc, JS::Value* vp, bool fulfilled) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject callee(cx, &args.callee());
  State& s = *static_cast<State*>(js::GetFunctionNativeReserved(callee, 0).toPrivate());
  const uint64_t id = uint64_t(js::GetFunctionNativeReserved(callee, 1).toNumber());
  args.rval().setUndefined();
  JS::RootedObject record(cx);
  if (!Record(s, id, &record) || !record) return true;
  JS::RootedValue promise(cx);
  if (!JS_GetElement(cx, record, ReqPromise, &promise) || !Forget(s, id)) return false;
  JS::RootedObject request(cx, &promise.toObject());
  if (!(fulfilled ? JS::ResolvePromise(cx, request, args.get(0)) : JS::RejectPromise(cx, request, args.get(0))))
    return false;
  std::string origin;
  if (!Origin(s, &origin)) return false;
  Registry& registry = *s.registry;
  registry.owners.erase(id);
  return DeliverGranted(registry, registry.queues[origin].Remove(id));
}
bool Fulfilled(JSContext* cx, unsigned argc, JS::Value* vp) { return Released(cx, argc, vp, true); }
bool Rejected(JSContext* cx, unsigned argc, JS::Value* vp) { return Released(cx, argc, vp, false); }
JSObject* Bound(State& s, JSNative native, uint64_t id) {
  JSFunction* function = js::NewFunctionWithReserved(s.cx, native, 1, 0, "");
  if (!function) return nullptr;
  JSObject* object = JS_GetFunctionObject(function);
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(&s));
  js::SetFunctionNativeReserved(object, 1, JS::NumberValue(double(id)));
  return object;
}
// The grant task: run the callback with a new Lock (or null when ifAvailable
// found the name busy), as LockRequestChild::RecvResolve does.
bool GrantTask(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject callee(cx, &args.callee());
  State& s = *static_cast<State*>(js::GetFunctionNativeReserved(callee, 0).toPrivate());
  const uint64_t id = uint64_t(js::GetFunctionNativeReserved(callee, 1).toNumber());
  args.rval().setUndefined();
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedObject record(cx);
  if (!Record(s, id, &record) || !record) return true;
  JS::RootedValue promise(cx), callback(cx), lockValue(cx), result(cx);
  if (!JS_GetElement(cx, record, ReqPromise, &promise) ||
      !JS_GetElement(cx, record, ReqCallback, &callback) ||
      !JS_GetElement(cx, record, ReqLock, &lockValue)) return false;
  JS::RootedObject request(cx, &promise.toObject());
  JS::RootedValueArray<1> values(cx);
  values[0].set(lockValue);
  const bool available = lockValue.isObject();
  if (!JS::Call(cx, JS::UndefinedHandleValue, callback, values, &result)) {
    JS::RootedValue error(cx);
    if (!JS_GetPendingException(cx, &error)) return false;
    JS_ClearPendingException(cx);
    if (!available) return Forget(s, id) && JS::RejectPromise(cx, request, error);
    JSObject* rejected = JS::CallOriginalPromiseReject(cx, error);
    if (!rejected) return false;
    result.setObject(*rejected);
  }
  if (!available) return Forget(s, id) && JS::ResolvePromise(cx, request, result);
  // Lock held until the callback's result settles (Lock's waiting promise).
  JS::RootedObject waiting(cx, JS::CallOriginalPromiseResolve(cx, result));
  JS::RootedObject onFulfilled(cx, Bound(s, Fulfilled, id)), onRejected(cx, Bound(s, Rejected, id));
  return waiting && onFulfilled && onRejected &&
         JS::AddPromiseReactions(cx, waiting, onFulfilled, onRejected);
}
bool Post(State& s, uint64_t id) {
  JS::RootedObject task(s.cx, Bound(s, GrantTask, id));
  if (!task) return false;
  return !s.taskPort || s.taskPort(s.taskContext, task);
}
bool Mode(JSContext* cx, JS::HandleValue options, bool* shared, bool* ifAvailable, bool* steal,
          bool* signal) {
  *shared = *ifAvailable = *steal = *signal = false;
  if (options.isNullOrUndefined()) return true;
  if (!options.isObject())
    return WebKit::Fail(cx, "LockManager.request: Argument 2 can't be converted to a dictionary.");
  JS::RootedObject object(cx, &options.toObject());
  JS::RootedValue value(cx);
  // Dictionary members in WebIDL (lexicographic) order.
  if (!JS_GetProperty(cx, object, "ifAvailable", &value)) return false;
  *ifAvailable = JS::ToBoolean(value);
  if (!JS_GetProperty(cx, object, "mode", &value)) return false;
  if (!value.isUndefined()) {
    JS::RootedString text(cx, JS::ToString(cx, value));
    std::string mode;
    if (!text || !ToUTF8(cx, text, &mode)) return false;
    if (mode != "shared" && mode != "exclusive")
      return WebKit::Fail(cx, ("LockManager.request: '" + mode +
          "' (value of 'mode' member of LockOptions) is not a valid value for enumeration LockMode.").c_str());
    *shared = mode == "shared";
  }
  if (!JS_GetProperty(cx, object, "signal", &value)) return false;
  *signal = !value.isUndefined();
  if (!JS_GetProperty(cx, object, "steal", &value)) return false;
  *steal = JS::ToBoolean(value);
  return true;
}
}  // namespace

bool Request(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* sp = Manager(cx, args, "request");
  if (!sp) return RejectPending(cx, args.rval());
  State& s = *sp;
  if (argc < 2)
    return WebKit::Fail(cx, ("LockManager.request: At least 2 arguments required, but only " +
                             std::to_string(argc) + " passed").c_str()) || RejectPending(cx, args.rval());
  JS::RootedString nameText(cx, JS::ToString(cx, args[0]));
  std::u16string name;
  if (!nameText) return RejectPending(cx, args.rval());
  name.resize(JS_GetStringLength(nameText));
  if (!JS_CopyStringChars(cx, mozilla::Range<char16_t>(name.data(), name.size()), nameText)) return false;
  JS::HandleValue options = argc > 2 ? args[1] : JS::UndefinedHandleValue;
  JS::HandleValue callback = argc > 2 ? args[2] : args[1];
  bool shared = false, ifAvailable = false, steal = false, signal = false;
  if (!Mode(cx, options, &shared, &ifAvailable, &steal, &signal)) return RejectPending(cx, args.rval());
  if (!callback.isObject() || !JS::IsCallable(&callback.toObject())) {
    (void)WebKit::Fail(cx, ("LockManager.request: Argument " + std::to_string(argc > 2 ? 3 : 2) +
                            " is not callable.").c_str());
    return RejectPending(cx, args.rval());
  }
  const char* invalid = !name.empty() && name[0] == u'-' ? "Names starting with `-` are reserved"
      : steal && ifAvailable ? "`steal` and `ifAvailable` cannot be used together"
      : steal && shared ? "`steal` is only supported for exclusive lock requests"
      : signal && steal ? "`steal` and `signal` cannot be used together"
      : signal && ifAvailable ? "`ifAvailable` and `signal` cannot be used together" : nullptr;
  if (invalid)
    return RejectWith(s, "NotSupportedError", (std::string("LockManager.request: ") + invalid).c_str(),
                      args.rval());
  std::string origin;
  if (!Origin(s, &origin)) return false;
  Registry& registry = *s.registry;
  const uint64_t id = registry.next++;
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  JS::RootedObject record(cx, JS::NewArrayObject(cx, ReqSlotCount));
  if (!promise || !record || !JS_SetElement(cx, record, ReqPromise, promise) ||
      !JS_SetElement(cx, record, ReqCallback, callback) ||
      !JS_DefineProperty(cx, s.requests, std::to_string(id).c_str(), record, JSPROP_ENUMERATE)) return false;
  registry.owners[id] = &s;
  locks::LockRequest entry{id, name, shared ? locks::LockMode::Shared : locks::LockMode::Exclusive, s.client};
  const auto step = registry.queues[origin].Enqueue(entry, ifAvailable, steal);
  args.rval().setObject(*promise);
  if (step.unavailable) {
    JS::RootedValue none(cx, JS::NullValue());
    return JS_SetElement(cx, record, ReqLock, none) && Post(s, id);
  }
  return Deliver(registry, step);
}

bool DeliverGranted(Registry& registry, const std::vector<locks::LockRequest>& granted) {
  for (const auto& entry : granted) {
    auto found = registry.owners.find(entry.id);
    if (found == registry.owners.end()) continue;
    State& s = *found->second;
    JSAutoRealm realm(s.cx, s.kit.page);
    JS::RootedObject record(s.cx);
    if (!Record(s, entry.id, &record) || !record) continue;
    JS::RootedObject lock(s.cx, s.kit.New(&kLockClass, s.lock.prototype));
    JS::RootedString name(s.cx, JS_NewUCStringCopyN(s.cx, entry.name.data(), entry.name.size()));
    if (!lock || !name) return false;
    JS::SetReservedSlot(lock, LockName, JS::StringValue(name));
    JS::SetReservedSlot(lock, LockMode, JS::BooleanValue(entry.mode == locks::LockMode::Shared));
    JS::RootedValue lockValue(s.cx, JS::ObjectValue(*lock));
    if (!JS_SetElement(s.cx, record, ReqLock, lockValue) || !Post(s, entry.id)) return false;
  }
  return true;
}
}  // namespace ruyijs::web_locks
