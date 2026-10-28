#include "web_media/media_codecs.h"
#include "web_media/media_configuration.h"
#include "web_media/state.h"
#include "web_media/media_runtime.h"
#include "runtime/native_constructor_trace.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/Exception.h"
#include "js/GlobalObject.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
namespace ruyijs::web_media {
const JSClass kCapabilitiesClass = {"MediaCapabilities", JSCLASS_HAS_RESERVED_SLOTS(1)};
namespace {
bool Result(State& state, JSContext* cx, const domain::web_media::Configuration& config,
            JS::MutableHandleValue out) {
  domain::web_media::Capability capability;
  const auto& track = config.video.present ? config.video : config.audio;
  if (!track.present || !ConfiguredDecoding(state, track.content_type, &capability))
    capability = domain::web_media::Evaluate(config);
  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  JS::RootedValue power(cx, JS::BooleanValue(capability.power_efficient));
  JS::RootedValue smooth(cx, JS::BooleanValue(capability.smooth));
  JS::RootedValue supported(cx, JS::BooleanValue(capability.supported));
  if (!result || !JS_DefineProperty(cx, result, "powerEfficient", power, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, result, "smooth", smooth, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, result, "supported", supported, JSPROP_ENUMERATE)) return false;
  JS::RootedValue null(cx, JS::NullValue());
  if (!config.encoding && !JS_DefineProperty(cx, result, "keySystemAccess", null, JSPROP_ENUMERATE)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*result));
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !JS::ResolvePromise(cx, promise, value)) return false;
  out.setObject(*promise); return true;
}
}
bool CapabilitiesConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
bool CapabilitiesMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const bool encoding = WebKit::Slot(args) == 1;
  JS::RootedObject caller(cx, JS::CurrentGlobalOrNull(cx));
  JS::RootedObject self(cx, args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!self || JS::GetClass(self) != &kCapabilitiesClass) {
    WebKit::Fail(cx, encoding ? "'encodingInfo' called on an object that does not implement interface MediaCapabilities." :
        "'decodingInfo' called on an object that does not implement interface MediaCapabilities.");
    return RejectMediaException(cx, caller, args.rval());
  }
  if (!argc) {
    WebKit::Fail(cx, encoding ? "MediaCapabilities.encodingInfo: At least 1 argument required, but only 0 passed" :
        "MediaCapabilities.decodingInfo: At least 1 argument required, but only 0 passed");
    return RejectMediaException(cx, caller, args.rval());
  }
  domain::web_media::Configuration config;
  // Binding conversion failures belong to the method realm. Semantic failures
  // and successful results belong to the receiver's realm (Firefox reference).
  if (!ReadMediaConfiguration(cx, args[0], encoding, &config)) return RejectMediaException(cx, caller, args.rval());
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, self);
    const char* error = !config.audio.present && !config.video.present ?
        "'audio' or 'video' member of argument of MediaCapabilities" : nullptr;
    for (bool video : {true, false}) if (!error)
      error = domain::web_media::ValidateTrack(video ? config.video : config.audio, video);
    if (error) {
      WebKit::Fail(cx, error);
      if (!RejectMediaException(cx, self, &result)) return false;
    } else if (!Result(*static_cast<State*>(JS::GetReservedSlot(self, 0).toPrivate()),
                      cx, config, &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool NavigatorCapabilities(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return browser_environment::Environment::ReadNavigatorMediaCapabilities(cx, args.thisv(), args.rval());
}
}
