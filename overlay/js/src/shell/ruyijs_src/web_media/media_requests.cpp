#include "web_media/media_constraints.h"
#include "web_media/media_runtime.h"
#include "jsapi.h"
#include "js/GlobalObject.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
namespace ruyijs::web_media {
bool MediaSupportedConstraints(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!MediaReceiver(cx, args.thisv(), &kMediaClass, &self)) return false;
  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  JS::RootedValue supported(cx, JS::BooleanValue(true));
  if (!result) return false;
  for (const auto& field : domain::web_media::kConstraintFields)
    if (!JS_DefineProperty(cx, result, field.name, supported, JSPROP_ENUMERATE)) return false;
  args.rval().setObject(*result); return true;
}
bool MediaDeviceRequest(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const bool display = WebKit::Slot(args) == 0;
  JS::RootedObject caller(cx, JS::CurrentGlobalOrNull(cx)), self(cx);
  if (!MediaReceiver(cx, args.thisv(), &kMediaClass, &self))
    return RejectMediaException(cx, caller, args.rval());
  if (!(display ? ReadDisplayConstraints(cx, args.get(0)) : ReadOutputOptions(cx, args.get(0))))
    return RejectMediaException(cx, caller, args.rval());
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, self);
    State* owner = StateForGlobal(JS::CurrentGlobalOrNull(cx));
    if (!owner) return WebKit::Fail(cx, "MediaDevices receiver realm is not installed");
    // The existing browser-environment activation baseline is inactive.
    // Conversion precedes Firefox's first business check; no permission/device
    // service is invented here and no overrideable JS activation getter is read.
    MediaException(*owner, "InvalidStateError", display ?
      "getDisplayMedia requires transient activation from a user gesture." :
      "selectAudioOutput requires transient user activation.");
    if (!RejectMediaException(cx, self, &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
}
