// PushManager, PushSubscription and PushSubscriptionOptions
// (docs/owners/SERVICE_WORKER.md, third slice). They hang off a
// ServiceWorkerRegistration, which this runtime never creates, so no instance
// exists; interfaces, member order, receiver checks and the static
// supportedContentEncodings follow Firefox.
#include "web_service_worker/internal.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <iterator>
#include <string>

namespace ruyijs::web_service_worker {
namespace {
enum Kind { Call, Promised, Get };
struct Member { const char* name; Kind kind; unsigned length; };
struct Interface { const char* name; const Member* members; size_t count; };
const Member kManager[] = {{"subscribe", Promised, 0}, {"getSubscription", Promised, 0},
                           {"permissionState", Promised, 0}};
const Member kSubscription[] = {{"getKey", Call, 1}, {"unsubscribe", Promised, 0},
                                {"toJSON", Call, 0}, {"endpoint", Get, 0},
                                {"options", Get, 0}, {"expirationTime", Get, 0}};
const Member kOptions[] = {{"applicationServerKey", Get, 0}};
const Interface kInterfaces[] = {{"PushManager", kManager, std::size(kManager)},
                                 {"PushSubscription", kSubscription, std::size(kSubscription)},
                                 {"PushSubscriptionOptions", kOptions, std::size(kOptions)}};
const JSClass kClasses[] = {
    {"PushManager", JSCLASS_HAS_RESERVED_SLOTS(std::size(kManager))},
    {"PushSubscription", JSCLASS_HAS_RESERVED_SLOTS(std::size(kSubscription))},
    {"PushSubscriptionOptions", JSCLASS_HAS_RESERVED_SLOTS(std::size(kOptions))}};
constexpr int32_t kStride = 64;
// Firefox's PushManager::GetSupportedContentEncodings: the one encoding it supports.
const char* const kEncodings[] = {"aes128gcm"};

bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
bool MemberNative(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const int32_t slot = WebKit::Slot(args);
  const Interface& face = kInterfaces[slot / kStride];
  const Member& m = face.members[slot % kStride];
  JSObject* self = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (!self || JS::GetClass(self) != &kClasses[slot / kStride]) {
    const std::string member = m.kind == Get ? std::string("get ") + m.name : std::string(m.name);
    (void)WebKit::Fail(cx, Incompatible(member.c_str(), face.name).c_str());
    if (m.kind != Promised) return false;
    JS::RootedValue error(cx);
    if (!JS_GetPendingException(cx, &error)) return false;
    JS_ClearPendingException(cx);
    return s.kit.RejectWith(error, args.rval());
  }
  // Unreachable until a subscription exists: no registration is ever created.
  args.rval().set(JS::GetReservedSlot(self, slot % kStride));
  return JS_WrapValue(cx, args.rval());
}
// [Exposed=Window,Worker] static readonly attribute FrozenArray<DOMString>.
bool SupportedEncodings(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  if (!array) return false;
  for (uint32_t i = 0; i < std::size(kEncodings); ++i) {
    JS::RootedString text(cx, JS_NewStringCopyZ(cx, kEncodings[i]));
    if (!text || !JS_DefineElement(cx, array, i, text, JSPROP_ENUMERATE)) return false;
  }
  if (!JS_FreezeObject(cx, array)) return false;
  args.rval().setObject(*array);
  return true;
}
}  // namespace

bool InstallPush(State& s) {
  JSContext* cx = s.cx;
  JS::RootedObject object(cx, JS::GetRealmObjectPrototype(cx));
  if (!object) return false;
  for (size_t i = 0; i < std::size(kInterfaces); ++i) {
    const Interface& face = kInterfaces[i];
    WebFace web(cx);
    if (!s.kit.Define(face.name, &kClasses[i], object, Illegal, &web)) return false;
    for (size_t j = 0; j < face.count; ++j) {
      const Member& m = face.members[j];
      const int32_t slot = int32_t(i) * kStride + int32_t(j);
      const bool ok = m.kind == Get
          ? s.kit.Accessor(web, face.name, m.name, MemberNative, nullptr, slot)
          : s.kit.Method(web, face.name, m.name, MemberNative, m.length, slot);
      if (!ok) return false;
    }
    if (i == 0) {
      JS::RootedObject getter(cx, s.kit.Function(SupportedEncodings,
                                                 "get supportedContentEncodings", 0, 0)), setter(cx);
      JS::RootedObject ctor(cx, web.constructor);
      if (!getter || !JS_DefineProperty(cx, ctor, "supportedContentEncodings", getter, setter,
                                        JSPROP_ENUMERATE) ||
          !s.kit.helpers->Member(getter, face.name, "get supportedContentEncodings", "get"))
        return false;
    }
    if (!s.secure.Add(s.kit.page, face.name)) return false;
  }
  return true;
}
}  // namespace ruyijs::web_service_worker
