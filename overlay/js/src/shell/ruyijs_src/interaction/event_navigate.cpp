#include "interaction/internal.h"
#include "runtime/page_helpers.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::interaction {
bool GetTrustedAcrossRealms(JSContext*, unsigned, JS::Value*);
namespace {
enum Field { NavigationType, Destination, CanIntercept, UserInitiated, HashChange,
             Signal, FormData, DownloadRequest, Info };
const char* const names[] = {"navigationType", "destination", "canIntercept", "userInitiated",
                             "hashChange", "signal", "formData", "downloadRequest", "info"};
bool Get(JSContext* cx, const JS::CallArgs& args, Field field) {
  Event* event = Receiver(cx, args, Iface::Navigate, true);
  if (!event) return false;
  ReportEventAccess(cx, event, "NavigateEvent", names[field], int(ApiAccess::Read));
  args.rval().setNull();
  if (field == Destination) {
    args.rval().set(JS::GetReservedSlot(event->object, kEventPublicFifth));
  } else if (field == NavigationType || field == DownloadRequest || field == Info) {
    const uint32_t slot = field == NavigationType ? kEventPublicFirst :
                          field == DownloadRequest ? kEventPublicSecond : kEventPublicThird;
    args.rval().set(JS::GetReservedSlot(event->object, slot));
  } else if (field >= CanIntercept && field <= HashChange) {
    const int32_t flags = JS::GetReservedSlot(event->object, kEventPublicFourth).toInt32();
    args.rval().setBoolean((flags & (1 << (field - CanIntercept))) != 0);
  }
  return JS_WrapValue(cx, args.rval());
}
#define NAVIGATE_GETTER(name, field) \
  bool name(JSContext* cx, unsigned n, JS::Value* vp) { \
    return Get(cx, JS::CallArgsFromVp(n, vp), field); \
  }
NAVIGATE_GETTER(GetNavigationType, NavigationType)
NAVIGATE_GETTER(GetDestination, Destination)
NAVIGATE_GETTER(GetCanIntercept, CanIntercept)
NAVIGATE_GETTER(GetUserInitiated, UserInitiated)
NAVIGATE_GETTER(GetHashChange, HashChange)
NAVIGATE_GETTER(GetSignal, Signal)
NAVIGATE_GETTER(GetFormData, FormData)
NAVIGATE_GETTER(GetDownloadRequest, DownloadRequest)
NAVIGATE_GETTER(GetInfo, Info)
#undef NAVIGATE_GETTER
bool Method(JSContext* cx, const JS::CallArgs& args, const char* name) {
  Event* event = Receiver(cx, args, Iface::Navigate, true);
  if (!event) return false;
  ReportEventAccess(cx, event, "NavigateEvent", name, int(ApiAccess::Call));
  args.rval().setUndefined();
  return true;
}
bool Intercept(JSContext* cx, unsigned n, JS::Value* vp) {
  return Method(cx, JS::CallArgsFromVp(n, vp), "intercept");
}
bool Scroll(JSContext* cx, unsigned n, JS::Value* vp) {
  return Method(cx, JS::CallArgsFromVp(n, vp), "scroll");
}
}
extern const JSPropertySpec kNavigateProperties[] = {
  JS_PSG("navigationType", GetNavigationType, JSPROP_ENUMERATE),
  JS_PSG("destination", GetDestination, JSPROP_ENUMERATE),
  JS_PSG("canIntercept", GetCanIntercept, JSPROP_ENUMERATE),
  JS_PSG("userInitiated", GetUserInitiated, JSPROP_ENUMERATE),
  JS_PSG("hashChange", GetHashChange, JSPROP_ENUMERATE),
  JS_PSG("signal", GetSignal, JSPROP_ENUMERATE),
  JS_PSG("formData", GetFormData, JSPROP_ENUMERATE),
  JS_PSG("downloadRequest", GetDownloadRequest, JSPROP_ENUMERATE),
  JS_PSG("info", GetInfo, JSPROP_ENUMERATE), JS_PS_END};
extern const JSFunctionSpec kNavigateMethods[] = {
  JS_FN("intercept", Intercept, 0, JSPROP_ENUMERATE),
  JS_FN("scroll", Scroll, 0, JSPROP_ENUMERATE), JS_FS_END};
bool InstallNavigateTrusted(State& state, JS::HandleObject object) {
  auto& getter = state.navigateTrustedGetter;
  if (!getter) {
    JSFunction* function = JS_NewFunction(state.cx, GetTrustedAcrossRealms, 0, 0, "get isTrusted");
    if (!function) return false;
    getter = JS_GetFunctionObject(function);
    if (!state.helpers->Member(getter, "NavigateEvent", "get isTrusted", "get")) return false;
  }
  JS::RootedObject setter(state.cx);
  return JS_DefineProperty(state.cx, object, "isTrusted", getter, setter,
                           JSPROP_ENUMERATE | JSPROP_PERMANENT);
}
}  // namespace ruyijs::interaction
