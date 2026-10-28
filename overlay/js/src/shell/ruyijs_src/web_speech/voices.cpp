// getVoices() and the SpeechSynthesis/SpeechSynthesisVoice getters
// (docs/owners/SPEECH_SYNTHESIS.md).
#include "web_speech/internal.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CharacterEncoding.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_speech {
const JSClass kVoiceClass = {"SpeechSynthesisVoice", JSCLASS_HAS_RESERVED_SLOTS(VoiceSlotCount)};
namespace {
// The one publication task: the platform's voice enumeration has finished,
// so getVoices() starts reporting it and `voiceschanged` fires.
bool Publish(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* s = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  args.rval().setUndefined();
  if (!s) return true;
  JSAutoRealm realm(cx, s->kit.page);
  s->voices->published = true;
  s->voices->publishing = false;
  s->voices->publisher = nullptr;
  return s->events.Fire(s->instance, "voiceschanged", false, false);
}
}
bool PostVoices(State& s) {
  JSContext* cx = s.cx;
  if (!s.taskPort || s.voices->published || s.voices->publishing) return true;
  // The task belongs to this realm's global: created while another (a retiring
  // predecessor's) realm is entered, the queue would drop it as retired.
  JSAutoRealm realm(cx, s.kit.page);
  JSFunction* function = js::NewFunctionWithReserved(cx, Publish, 0, 0, "voiceschanged");
  if (!function) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(&s));
  if (!s.taskPort(s.taskContext, task, 0)) return false;
  s.voices->publishing = true;
  s.voices->publisher = &s;
  return true;
}
JSObject* Queue(State& s) { return &JS::GetReservedSlot(s.instance, SynthQueue).toObject(); }
bool Voices(State& s, JS::MutableHandleValue out) {
  JSContext* cx = s.cx;
  // Before the platform enumeration lands, getVoices() is empty (Firefox).
  const auto& list = s.voices->voices;
  if (!s.voices->published) {
    // The platform enumeration starts on first use (Firefox creates its voice
    // registry then): this call reports [] and `voiceschanged` follows in a task.
    if (!PostVoices(s)) return false;
    JS::RootedObject empty(cx, JS::NewArrayObject(cx, 0));
    if (!empty) return false;
    out.setObject(*empty);
    return true;
  }
  JS::RootedObject array(cx, JS::NewArrayObject(cx, list.size()));
  if (!array) return false;
  for (size_t i = 0; i < list.size(); ++i) {
    JS::RootedObject voice(cx, s.kit.New(&kVoiceClass, s.voice.prototype));
    const std::string uri = "urn:moz-tts:sapi:" + list[i].name + "?" + list[i].lang;
    JS::RootedString name(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(list[i].name.data(), list[i].name.size())));
    JS::RootedString lang(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(list[i].lang.data(), list[i].lang.size())));
    JS::RootedString urn(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(uri.data(), uri.size())));
    if (!voice || !name || !lang || !urn) return false;
    JS::SetReservedSlot(voice, VoiceUri, JS::StringValue(urn));
    JS::SetReservedSlot(voice, VoiceName, JS::StringValue(name));
    JS::SetReservedSlot(voice, VoiceLang, JS::StringValue(lang));
    JS::SetReservedSlot(voice, VoiceLocal, JS::TrueValue());
    JS::SetReservedSlot(voice, VoiceDefault, JS::BooleanValue(list[i].name == s.voices->defaultName));
    JS::RootedValue item(cx, JS::ObjectValue(*voice));
    if (!JS_DefineElement(cx, array, uint32_t(i), item, JSPROP_ENUMERATE)) return false;
  }
  out.setObject(*array);
  return true;
}
State& Owner(JS::HandleObject self) {
  return *static_cast<State*>(JS::GetReservedSlot(self, SynthState).toPrivate());
}

bool SynthesisGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  static const char* const names[] = {"get pending", "get speaking", "get paused", "get onvoiceschanged"};
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kSynthesisClass, names[slot], &self)) return false;
  State& s = Owner(self);
  if (slot == 3) {
    args.rval().set(JS::GetReservedSlot(self, SynthHandler));
    return JS_WrapValue(cx, args.rval());
  }
  uint32_t queued = 0;
  JS::RootedObject queue(cx, Queue(s));
  if (slot == 0 && !JS::GetArrayLength(cx, queue, &queued)) return false;
  args.rval().setBoolean(slot == 0 ? queued > 0 : slot == 1 ? s.speaking : s.paused);
  return true;
}
bool SynthesisHandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kSynthesisClass, "set onvoiceschanged", &self)) return false;
  JS::RootedValue value(cx, args.get(0));
  args.rval().setUndefined();
  return SetHandler(Owner(self), self, "voiceschanged", SynthHandler, value);
}
bool VoiceGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  static const char* const names[] = {"get voiceURI", "get name", "get lang", "get localService", "get default"};
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kVoiceClass, names[slot], &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, slot));
  return JS_WrapValue(cx, args.rval());
}
}  // namespace ruyijs::web_speech
