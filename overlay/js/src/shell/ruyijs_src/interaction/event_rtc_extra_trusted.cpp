#include "interaction/internal.h"
#include "runtime/page_helpers.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::interaction {
bool GetTrustedAcrossRealms(JSContext*, unsigned, JS::Value*);
bool InstallRtcExtraTrusted(State& state, JS::HandleObject object, Kind kind) {
  auto& getter = kind == Kind::RtcTone ? state.rtcToneTrustedGetter :
                 kind == Kind::RtcErrorPayload ? state.rtcErrorTrustedGetter : state.rtcIceErrorTrustedGetter;
  const char* name = kind == Kind::RtcTone ? "RTCDTMFToneChangeEvent" :
                     kind == Kind::RtcErrorPayload ? "RTCErrorEvent" : "RTCPeerConnectionIceErrorEvent";
  if (!getter) {
    JSFunction* function = JS_NewFunction(state.cx, GetTrustedAcrossRealms, 0, 0, "get isTrusted");
    if (!function) return false;
    getter = JS_GetFunctionObject(function);
    if (!state.helpers->Member(getter, name, "get isTrusted", "get")) return false;
  }
  JS::RootedObject setter(state.cx);
  return JS_DefineProperty(state.cx, object, "isTrusted", getter, setter,
                           JSPROP_ENUMERATE | JSPROP_PERMANENT);
}
}  // namespace ruyijs::interaction
