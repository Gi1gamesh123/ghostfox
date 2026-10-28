// ServiceWorker, ServiceWorkerRegistration and NavigationPreloadManager
// (docs/owners/SERVICE_WORKER.md, second slice). This runtime runs no service
// worker and register() always rejects, so no instance of these interfaces is
// ever created; the interfaces, member order, receiver checks and error forms
// follow Firefox. Each instance would keep its values in reserved slots.
#include "web_service_worker/internal.h"
#include "interaction/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <iterator>
#include <string>

namespace ruyijs::web_service_worker {
namespace {
enum Kind { Call, Promised, Get, Handler };
struct Member { const char* name; Kind kind; unsigned length; };
struct Interface {
  const char* name;
  bool eventTarget;
  const Member* members;
  size_t count;
};
// Firefox prototype order (dom/webidl), operations first, then attributes.
const Member kWorker[] = {{"postMessage", Call, 1}, {"scriptURL", Get, 0}, {"state", Get, 0},
                          {"statechange", Handler, 0}, {"error", Handler, 0}};
const Member kRegistration[] = {
    {"update", Promised, 0}, {"unregister", Promised, 0}, {"showNotification", Promised, 1},
    {"getNotifications", Promised, 0}, {"installing", Get, 0}, {"waiting", Get, 0},
    {"active", Get, 0}, {"navigationPreload", Get, 0}, {"scope", Get, 0},
    {"updateViaCache", Get, 0}, {"updatefound", Handler, 0}, {"pushManager", Get, 0},
    {"cookies", Get, 0}};
const Member kPreload[] = {{"enable", Promised, 0}, {"disable", Promised, 0},
                           {"setHeaderValue", Promised, 1}, {"getState", Promised, 0}};
const Interface kInterfaces[] = {
    {"ServiceWorker", true, kWorker, std::size(kWorker)},
    {"ServiceWorkerRegistration", true, kRegistration, std::size(kRegistration)},
    {"NavigationPreloadManager", false, kPreload, std::size(kPreload)}};
const JSClass kClasses[] = {
    {"ServiceWorker", JSCLASS_HAS_RESERVED_SLOTS(std::size(kWorker))},
    {"ServiceWorkerRegistration", JSCLASS_HAS_RESERVED_SLOTS(std::size(kRegistration))},
    {"NavigationPreloadManager", JSCLASS_HAS_RESERVED_SLOTS(std::size(kPreload))}};
constexpr int32_t kStride = 64;  // function slot = interface * kStride + member index

bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
// The member's display name in Firefox's receiver error.
std::string Display(const Member& m, bool setter) {
  if (m.kind == Handler) return std::string(setter ? "set on" : "get on") + m.name;
  if (m.kind == Get) return std::string("get ") + m.name;
  return m.name;
}
// The receiver for the member at |slot|, or a pending TypeError.
JSObject* Receiver(JSContext* cx, const JS::CallArgs& args, int32_t slot, bool setter,
                   const Member** member) {
  const Interface& face = kInterfaces[slot / kStride];
  *member = &face.members[slot % kStride];
  JSObject* self = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (self && JS::GetClass(self) == &kClasses[slot / kStride]) return self;
  (void)WebKit::Fail(cx, Incompatible(Display(**member, setter).c_str(), face.name).c_str());
  return nullptr;
}
bool MemberNative(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const int32_t slot = WebKit::Slot(args);
  const Member* m = nullptr;
  JS::RootedObject self(cx, Receiver(cx, args, slot, false, &m));
  if (!self) return m->kind == Promised ? Reject(s, args.rval()) : false;
  // Instances exist only once a registry created them (objects.cpp).
  if (m->kind == Call || m->kind == Promised)
    return CallMember(s, ObjectKind(slot / kStride), m->name, self, args);
  args.rval().set(JS::GetReservedSlot(self, slot % kStride));
  return JS_WrapValue(cx, args.rval());
}
bool MemberHandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const int32_t slot = WebKit::Slot(args);
  const Member* m = nullptr;
  JS::RootedObject self(cx, Receiver(cx, args, slot, true, &m));
  if (!self) return false;
  const uint32_t index = uint32_t(slot % kStride);
  JS::RootedValue previous(cx, JS::GetReservedSlot(self, index)), desired(cx, args.get(0));
  if (!desired.isObject() || !JS::IsCallable(&desired.toObject())) desired.setNull();
  JSAutoRealm realm(cx, s.kit.page);
  if (!JS_WrapValue(cx, &desired) || !JS_WrapValue(cx, &previous)) return false;
  JS::RootedObject prototype(cx, s.events.prototype());
  JS::RootedValue function(cx), receiver(cx, JS::ObjectValue(*self)), ignored(cx);
  JS::RootedString type(cx, JS_NewStringCopyZ(cx, m->name));
  if (!type) return false;
  JS::RootedValueArray<2> parameters(cx);
  parameters[0].setString(type);
  for (int operation = 0; operation < 2; ++operation) {
    parameters[1].set(operation ? desired : previous);
    if (!parameters[1].isObject()) continue;
    if (!JS_GetProperty(cx, prototype, operation ? "addEventListener" : "removeEventListener", &function) ||
        !JS::Call(cx, receiver, function, parameters, &ignored)) return false;
  }
  JS::SetReservedSlot(self, index, desired);
  args.rval().setUndefined();
  return true;
}
}  // namespace

JSObject* NewObject(State& s, ObjectKind kind) {
  JS::RootedObject prototype(s.cx, s.prototypes[kind]);
  JSObject* object = s.kit.New(&kClasses[kind], prototype);
  if (object && kInterfaces[kind].eventTarget) {
    JS::RootedObject target(s.cx, object);
    if (!s.events.Attach(target)) return nullptr;
    for (size_t j = 0; j < kInterfaces[kind].count; ++j)
      if (kInterfaces[kind].members[j].kind == Handler) JS::SetReservedSlot(object, uint32_t(j), JS::NullValue());
  }
  return object;
}
bool InstallRegistration(State& s) {
  JSContext* cx = s.cx;
  JS::RootedObject target(cx, s.events.prototype()), object(cx, JS::GetRealmObjectPrototype(cx));
  if (!target || !object) return false;
  for (size_t i = 0; i < std::size(kInterfaces); ++i) {
    const Interface& face = kInterfaces[i];
    WebFace web(cx);
    if (!s.kit.Define(face.name, &kClasses[i], face.eventTarget ? target : object, Illegal, &web))
      return false;
    s.prototypes[i] = web.prototype;
    for (size_t j = 0; j < face.count; ++j) {
      const Member& m = face.members[j];
      const int32_t slot = int32_t(i) * kStride + int32_t(j);
      bool ok;
      if (m.kind == Handler) {
        const std::string name = std::string("on") + m.name;
        ok = s.kit.Accessor(web, face.name, name.c_str(), MemberNative, MemberHandlerSet, slot);
      } else if (m.kind == Get) {
        ok = s.kit.Accessor(web, face.name, m.name, MemberNative, nullptr, slot);
      } else {
        ok = s.kit.Method(web, face.name, m.name, MemberNative, m.length, slot);
      }
      if (!ok) return false;
    }
    if (!s.secure.Add(s.kit.page, face.name)) return false;
  }
  return true;
}
}  // namespace ruyijs::web_service_worker
