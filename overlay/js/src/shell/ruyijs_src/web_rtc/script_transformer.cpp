// RTCRtpScriptTransformer (Exposed=DedicatedWorker). Firefox creates one only
// for the worker's `rtctransform` event when a page's RTCRtpScriptTransform
// is attached to a negotiated sender/receiver; this owner runs no media
// pipeline (D136), so no instance exists. The interface keeps Firefox 158's
// shape: an illegal constructor, generateKeyFrame/sendKeyFrameRequest (promise
// operations: a foreign receiver rejects) and readable/writable/options.
#include "web_rtc/state.h"

#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::web_rtc {
namespace {
const JSClass kScriptTransformerClass = {"RTCRtpScriptTransformer", JSCLASS_HAS_RESERVED_SLOTS(1)};
const char* const kName = "RTCRtpScriptTransformer";

bool Operation(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  JS::RootedValue error(cx);
  // With no instance the brand check always fails; the operation reports it
  // as a rejected promise, as WebIDL does for a Promise-returning operation.
  if (Brand(cx, args, &kScriptTransformerClass, &self))
    return WebKit::Fail(cx, "RTCRtpScriptTransformer has no media pipeline");
  if (!JS_GetPendingException(cx, &error)) return false;
  JS_ClearPendingException(cx);
  return state.kit.RejectWith(error, args.rval());
}

bool Attribute(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kScriptTransformerClass, &self)) return false;
  return WebKit::Fail(cx, "RTCRtpScriptTransformer has no media pipeline");
}
}  // namespace

bool InstallScriptTransformer(State& s, JS::HandleObject objectProto) {
  auto& kit = s.kit;
  return kit.Define(kName, &kScriptTransformerClass, objectProto, WebKit::Reject, &s.scriptTransformer) &&
         kit.Method(s.scriptTransformer, kName, "generateKeyFrame", Operation, 0, 0) &&
         kit.Method(s.scriptTransformer, kName, "sendKeyFrameRequest", Operation, 0, 1) &&
         kit.Accessor(s.scriptTransformer, kName, "readable", Attribute, nullptr, 0) &&
         kit.Accessor(s.scriptTransformer, kName, "writable", Attribute, nullptr, 1) &&
         kit.Accessor(s.scriptTransformer, kName, "options", Attribute, nullptr, 2);
}

// Brand 3 (kBrandRtcScriptTransformer) for RTCTransformEventInit.transformer.
bool IsScriptTransformer(JSObject* object) {
  object = js::CheckedUnwrapStatic(object);
  return object && JS::GetClass(object) == &kScriptTransformerClass;
}
}  // namespace ruyijs::web_rtc
