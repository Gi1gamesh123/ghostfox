// SpeechSynthesisUtterance: the public constructor and its attributes
// (docs/owners/SPEECH_SYNTHESIS.md). volume/rate/pitch are clamped and stored
// as float, as Firefox does; non-finite values throw.
#include "web_speech/internal.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Wrapper.h"
#include <cmath>
#include <string>

namespace ruyijs::web_speech {
const JSClass kUtteranceClass = {"SpeechSynthesisUtterance", JSCLASS_HAS_RESERVED_SLOTS(UttSlotCount)};
namespace {
const char* const kAttributes[] = {"text", "lang", "voice", "volume", "rate", "pitch"};
std::string Member(int32_t slot) {
  return slot < UttHandlerFirst ? kAttributes[slot] : std::string("on") + kHandlers[slot - UttHandlerFirst];
}
bool Number(JSContext* cx, int32_t slot, JS::HandleValue value, JS::MutableHandleValue out) {
  double number = 0;
  if (!JS::ToNumber(cx, value, &number)) return false;
  if (!std::isfinite(number))
    return WebKit::Fail(cx, ("SpeechSynthesisUtterance." + Member(slot) +
                             " setter: Value being assigned is not a finite floating-point value.").c_str());
  const double low = slot == UttRate ? 0.1 : 0, high = slot == UttVolume ? 1 : slot == UttRate ? 10 : 2;
  out.setDouble(double(float(std::fmin(std::fmax(number, low), high))));
  return true;
}
}
bool UtteranceConstruct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  if (!args.isConstructing())
    return WebKit::Fail(cx, "SpeechSynthesisUtterance constructor: 'new' is required");
  JS::RootedObject fallback(cx, s.utterance.prototype), proto(cx);
  if (!s.kit.ConstructProto(args, fallback, &proto)) return false;
  JS::RootedString text(cx, JS_GetEmptyString(cx));
  if (args.length() && !args[0].isUndefined() && !(text = JS::ToString(cx, args[0]))) return false;
  JS::RootedObject self(cx, s.kit.New(&kUtteranceClass, proto));
  if (!self) return false;
  JS::SetReservedSlot(self, UttText, JS::StringValue(text));
  JS::SetReservedSlot(self, UttLang, JS::StringValue(JS_GetEmptyString(cx)));
  JS::SetReservedSlot(self, UttVoice, JS::NullValue());
  for (int32_t slot : {UttVolume, UttRate, UttPitch}) JS::SetReservedSlot(self, slot, JS::DoubleValue(1));
  for (int32_t slot = UttHandlerFirst; slot < UttSlotCount; ++slot) JS::SetReservedSlot(self, slot, JS::NullValue());
  if (!s.events.Attach(self)) return false;
  args.rval().setObject(*self);
  return true;
}
bool UtteranceGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kUtteranceClass, "get " + Member(slot), &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, slot));
  return JS_WrapValue(cx, args.rval());
}
bool UtteranceSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kUtteranceClass, "set " + Member(slot), &self)) return false;
  JS::RootedValue value(cx, args.get(0)), stored(cx);
  args.rval().setUndefined();
  if (slot >= UttHandlerFirst)
    return SetHandler(s, self, kHandlers[slot - UttHandlerFirst], uint32_t(slot), value);
  if (slot == UttText || slot == UttLang) {
    JSString* text = JS::ToString(cx, value);
    if (!text) return false;
    stored.setString(text);
  } else if (slot == UttVoice) {
    JSObject* voice = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
    if (!value.isNull() && (!voice || JS::GetClass(voice) != &kVoiceClass))
      return WebKit::Fail(cx, "SpeechSynthesisUtterance.voice setter: Value being assigned does not "
                              "implement interface SpeechSynthesisVoice.");
    stored.set(value);
  } else if (!Number(cx, slot, value, &stored)) {
    return false;
  }
  JSAutoRealm realm(cx, self);
  if (!JS_WrapValue(cx, &stored)) return false;
  JS::SetReservedSlot(self, slot, stored);
  return true;
}
}  // namespace ruyijs::web_speech
