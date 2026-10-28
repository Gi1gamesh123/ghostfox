// Navigator members that belong to the Web Media owner but are not on
// MediaDevices: getAutoplayPolicy (Navigator.webidl autoplay detection, three
// overloads) and the legacy callback mozGetUserMedia. Values are the ones
// Firefox 158 reports to content with default autoplay prefs and no user
// gesture: "allowed-muted" for media elements and "mediaelement", "disallowed"
// for audio contexts. mozGetUserMedia shares the MediaDevices constraint rule
// (docs/owners/MEDIA_DEVICES.md): without audio/video the error callback runs
// as a microtask with Firefox's MediaStreamError; a real request waits for a
// permission prompt this runtime never shows, so neither callback runs.
#include "web_media/realm.h"
#include "web_media/state.h"
#include "web_media/media_runtime.h"
#include "runtime/runtime.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
#include <string>

namespace ruyijs::web_media {
namespace {
bool Navigator(JSContext* cx, const JS::CallArgs& args, State** state, const char* member) {
  *state = static_cast<State*>(WebKit::Owner(args));
  JSObject* self = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (self && (*state)->navigator && JS::GetClass(self) == JS::GetClass((*state)->navigator)) return true;
  return WebKit::Fail(cx, ("'" + std::string(member) +
      "' called on an object that does not implement interface Navigator.").c_str());
}
bool Text(JSContext* cx, const char* text, JS::MutableHandleValue out) {
  JSString* string = JS_NewStringCopyZ(cx, text);
  if (!string) return false;
  out.setString(string);
  return true;
}
bool GetAutoplayPolicy(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* s = nullptr;
  if (!Navigator(cx, args, &s, "getAutoplayPolicy")) return false;
  const std::string prefix = "Navigator.getAutoplayPolicy: ";
  if (!argc) return WebKit::Fail(cx, (prefix + "0 is not a valid argument count for any overload.").c_str());
  // Overload resolution: interface brands first, then the enum conversion.
  if (s->mediaElementBrand && s->mediaElementBrand(args[0]))
    return Text(cx, "allowed-muted", args.rval());
  if (s->audioContextBrand && s->audioContextBrand(args[0]))
    return Text(cx, "disallowed", args.rval());
  JS::RootedString type(cx, JS::ToString(cx, args[0]));
  std::string value;
  if (!type || !ToUTF8(cx, type, &value)) return false;
  if (value == "mediaelement") return Text(cx, "allowed-muted", args.rval());
  if (value == "audiocontext") return Text(cx, "disallowed", args.rval());
  return WebKit::Fail(cx, (prefix + "'" + value +
      "' (value of argument 1) is not a valid value for enumeration AutoplayPolicyMediaType.").c_str());
}
// Promise reaction that calls the saved error callback with the saved error.
bool DeliverError(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject callee(cx, &args.callee());
  JS::RootedValue callback(cx, js::GetFunctionNativeReserved(callee, 0)),
      error(cx, js::GetFunctionNativeReserved(callee, 1)), ignored(cx);
  args.rval().setUndefined();
  JS::RootedValueArray<1> values(cx);
  values[0].set(error);
  return JS::Call(cx, JS::UndefinedHandleValue, callback, values, &ignored);
}
bool MozGetUserMedia(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* s = nullptr;
  if (!Navigator(cx, args, &s, "mozGetUserMedia")) return false;
  const std::string prefix = "Navigator.mozGetUserMedia: ";
  if (argc < 3)
    return WebKit::Fail(cx, (prefix + "At least 3 arguments required, but only " +
                             std::to_string(argc) + " passed").c_str());
  if (!args[0].isNullOrUndefined() && !args[0].isObject())
    return WebKit::Fail(cx, (prefix + "Argument 1 can't be converted to a dictionary.").c_str());
  bool requested = false;
  if (args[0].isObject()) {
    JS::RootedObject constraints(cx, &args[0].toObject());
    for (const char* key : {"audio", "video"}) {
      JS::RootedValue value(cx);
      if (!JS_GetProperty(cx, constraints, key, &value)) return false;
      requested |= JS::ToBoolean(value);
    }
  }
  for (unsigned i = 1; i < 3; ++i)
    if (!args[i].isObject() || !JS::IsCallable(&args[i].toObject()))
      return WebKit::Fail(cx, (prefix + "Argument " + std::to_string(i + 1) + " is not an object.").c_str());
  args.rval().setUndefined();
  if (requested) return true;
  JS::RootedObject error(cx);
  if (!NewMediaStreamError(*s, "TypeError", "audio and/or video is required", "", &error)) return false;
  JS::RootedObject reaction(cx, JS_GetFunctionObject(js::NewFunctionWithReserved(cx, DeliverError, 0, 0, "")));
  if (!reaction) return false;
  js::SetFunctionNativeReserved(reaction, 0, args[2]);
  js::SetFunctionNativeReserved(reaction, 1, JS::ObjectValue(*error));
  // Firefox settles the MediaManager promise and then runs the callback, one
  // microtask after the caller's own already-queued reactions.
  JS::RootedObject settled(cx, JS::CallOriginalPromiseResolve(cx, JS::UndefinedHandleValue));
  JS::RootedObject hop(cx, settled ? JS::CallOriginalPromiseThen(cx, settled, nullptr, nullptr) : nullptr);
  return hop && JS::AddPromiseReactions(cx, hop, reaction, nullptr);
}
}  // namespace

void Realm::SetAutoplayBrandPorts(BrandPort mediaElement, BrandPort audioContext) {
  state_->mediaElementBrand = mediaElement;
  state_->audioContextBrand = audioContext;
}

bool InstallNavigatorMedia(State& s) {
  JS::RootedObject proto(s.cx, s.environment.navigatorPrototype());
  struct Spec { const char* name; JSNative native; unsigned length; };
  for (const Spec& spec : {Spec{"mozGetUserMedia", MozGetUserMedia, 3},
                           Spec{"getAutoplayPolicy", GetAutoplayPolicy, 1}}) {
    JS::RootedObject method(s.cx, s.kit.Function(spec.native, spec.name, spec.length, 0));
    if (!proto || !method || !JS_DefineProperty(s.cx, proto, spec.name, method, JSPROP_ENUMERATE) ||
        !s.kit.helpers->Member(method, "Navigator", spec.name, "call")) return false;
  }
  return true;
}

bool Realm::ConfigureExposure() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedValue secure(s.cx);
  return JS_GetProperty(s.cx, s.kit.page, "isSecureContext", &secure) && s.secure.Apply(secure.isTrue());
}
}  // namespace ruyijs::web_media
