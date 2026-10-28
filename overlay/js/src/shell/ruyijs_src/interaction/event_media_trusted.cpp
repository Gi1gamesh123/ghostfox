#include "interaction/internal.h"
#include "runtime/page_helpers.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::interaction {
bool GetTrustedAcrossRealms(JSContext*, unsigned, JS::Value*);
namespace {
bool EncryptedTrusted(JSContext* cx, unsigned n, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(n, vp);
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  State* owner = object ? Owner(cx, object, true) : nullptr;
  Event* event = owner ? owner->event(object) : nullptr;
  args.rval().setUndefined();
  if (event && event->kind == Kind::MediaEncrypted) {
    ReportEventAccess(cx, event, "MediaEncryptedEvent", "isTrusted", int(ApiAccess::Read));
    args.rval().setBoolean(false);
  }
  return true;
}
bool KeyTrusted(JSContext* cx, unsigned n, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(n, vp);
  if (State* owner = RealmState(cx))
    owner->Observe("MediaKeyMessageEvent", "isTrusted", int(ApiAccess::Read));
  args.rval().setBoolean(false);
  return true;
}
}
bool InstallMediaEventTrusted(State& state, JS::HandleObject object, Kind kind) {
  const bool encrypted = kind == Kind::MediaEncrypted, key = kind == Kind::MediaKeyMessage;
  const bool stream = kind == Kind::MediaStream;
  auto& getter = encrypted ? state.mediaEncryptedTrustedGetter : key ? state.mediaKeyTrustedGetter :
                 stream ? state.mediaStreamTrustedGetter : state.mediaTrackTrustedGetter;
  const char* name = encrypted ? "MediaEncryptedEvent" : key ? "MediaKeyMessageEvent" :
                     stream ? "MediaStreamEvent" : "TrackEvent";
  if (!getter) {
    JSFunction* function = JS_NewFunction(state.cx, encrypted ? EncryptedTrusted :
        key ? KeyTrusted : GetTrustedAcrossRealms, 0, 0, "get isTrusted");
    if (!function) return false;
    getter = JS_GetFunctionObject(function);
    if (!state.helpers->Member(getter, name, "get isTrusted", "get")) return false;
  }
  JS::RootedObject setter(state.cx);
  return JS_DefineProperty(state.cx, object, "isTrusted", getter, setter,
                           JSPROP_ENUMERATE | (key || stream ? JSPROP_PERMANENT : 0));
}
}  // namespace ruyijs::interaction
