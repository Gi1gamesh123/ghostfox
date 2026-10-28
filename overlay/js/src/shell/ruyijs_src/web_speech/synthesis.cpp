// SpeechSynthesis methods and the speaking steps (docs/owners/SPEECH_SYNTHESIS.md):
// start, a sentence boundary, one word boundary per word, then end, one task
// each. No audio: the steps only drive the events Firefox fires.
#include "web_speech/internal.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include <string>
#include <vector>

namespace ruyijs::web_speech {
const JSClass kSynthesisClass = {"SpeechSynthesis", JSCLASS_HAS_RESERVED_SLOTS(SynthSlotCount)};
namespace {
constexpr int32_t kBegin = -1, kCancelEnd = -2;  // Step markers besides event indices.
const char* const kMethods[] = {"speak", "cancel", "pause", "resume", "getVoices"};
double Now(State& s) {
  JS::RootedValue date(s.cx, JS::ObjectValue(*s.date)), now(s.cx, JS::ObjectValue(*s.dateNow)), out(s.cx);
  return JS::Call(s.cx, date, now, JS::HandleValueArray::empty(), &out) && out.isNumber()
      ? out.toNumber() : 0;
}
bool Post(State& s, int32_t step, double delay = 0);
// Word spans of the current text (UTF-16 offsets), whitespace-separated.
std::vector<std::pair<uint32_t, uint32_t>> Words(JSContext* cx, JSString* text) {
  std::vector<std::pair<uint32_t, uint32_t>> words;
  const size_t length = JS_GetStringLength(text);
  uint32_t start = 0;
  bool inWord = false;
  for (size_t i = 0; i <= length; ++i) {
    char16_t c = u' ';
    if (i < length && !JS_GetStringCharAt(cx, text, i, &c)) break;
    const bool space = c == u' ' || c == u'\t' || c == u'\n' || c == u'\r';
    if (!space && !inWord) { start = uint32_t(i); inWord = true; }
    if (space && inWord) { words.emplace_back(start, uint32_t(i) - start); inWord = false; }
  }
  return words;
}
bool Fire(State& s, JS::HandleObject utterance, const char* type, uint32_t index,
          JS::HandleValue length, const char* name) {
  JSContext* cx = s.cx;
  JS::RootedObject init(cx, JS_NewPlainObject(cx));
  JS::RootedString label(cx, JS_NewStringCopyZ(cx, name));
  if (!init || !label) return false;
  JS::RootedValue target(cx, JS::ObjectValue(*utterance)), text(cx, JS::StringValue(label));
  JS::RootedValue elapsed(cx, JS::DoubleValue(s.started ? (Now(s) - s.started) / 1000.0 : 0));
  return JS_DefineProperty(cx, init, "utterance", target, JSPROP_ENUMERATE) &&
         JS_DefineProperty(cx, init, "charIndex", index, JSPROP_ENUMERATE) &&
         JS_DefineProperty(cx, init, "charLength", length, JSPROP_ENUMERATE) &&
         JS_DefineProperty(cx, init, "elapsedTime", elapsed, JSPROP_ENUMERATE) &&
         JS_DefineProperty(cx, init, "name", text, JSPROP_ENUMERATE) &&
         s.events.FireSpeech(utterance, type, init, false);
}
bool Finish(State& s, JS::HandleObject utterance, uint32_t length) {
  if (!Fire(s, utterance, "end", length, JS::NullHandleValue, "")) return false;
  JS::SetReservedSlot(s.instance, SynthCurrent, JS::NullValue());
  s.speaking = false;
  s.started = 0;
  uint32_t queued = 0;
  JS::RootedObject queue(s.cx, Queue(s));
  return JS::GetArrayLength(s.cx, queue, &queued) && (!queued || Post(s, kBegin));
}
// One step of the current run. Steps of a cancelled run (older generation) are dropped.
bool Step(State& s, int32_t step) {
  JSContext* cx = s.cx;
  JS::RootedObject queue(cx, Queue(s));
  if (step == kBegin) {
    uint32_t length = 0;
    JS::RootedValue first(cx);
    if (!JS::GetArrayLength(cx, queue, &length)) return false;
    if (!length || s.speaking) return true;
    JS::RootedObject rest(cx, JS::NewArrayObject(cx, 0));
    if (!rest || !JS_GetElement(cx, queue, 0, &first)) return false;
    for (uint32_t i = 1; i < length; ++i) {
      JS::RootedValue item(cx);
      if (!JS_GetElement(cx, queue, i, &item) || !JS_SetElement(cx, rest, i - 1, item)) return false;
    }
    JS::SetReservedSlot(s.instance, SynthQueue, JS::ObjectValue(*rest));
    JS::SetReservedSlot(s.instance, SynthCurrent, first);
    s.speaking = true;
    return Post(s, 0, 200);
  }
  const JS::Value current = JS::GetReservedSlot(s.instance, SynthCurrent);
  if (!current.isObject()) return true;
  JS::RootedObject utterance(cx, &current.toObject());
  JS::RootedString text(cx, JS::GetReservedSlot(utterance, UttText).toString());
  const uint32_t length = uint32_t(JS_GetStringLength(text));
  if (step == kCancelEnd) {
    // Firefox still reports start for an utterance cancelled before it spoke.
    if (!s.started && !Fire(s, utterance, "start", 0, JS::NullHandleValue, "")) return false;
    return Finish(s, utterance, length);
  }
  const auto words = Words(cx, text);
  const double rate = JS::GetReservedSlot(utterance, UttRate).toDouble();
  const double word = 300 / (rate > 0 ? rate : 1);
  if (step == 0) {
    s.started = Now(s);
    return Fire(s, utterance, "start", 0, JS::NullHandleValue, "") && Post(s, 1, 10);
  }
  if (step == 1 && length) {
    JS::RootedValue span(cx, JS::Int32Value(int32_t(length)));
    return Fire(s, utterance, "boundary", 0, span, "sentence") && Post(s, 2, 10);
  }
  const size_t index = size_t(step) - 2;
  if (length && index < words.size()) {
    JS::RootedValue span(cx, JS::Int32Value(int32_t(words[index].second)));
    return Fire(s, utterance, "boundary", words[index].first, span, "word") && Post(s, step + 1, word);
  }
  return Finish(s, utterance, length);
}
bool RunStep(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  // Read the callee's slots first: rval() shares its Value with callee().
  State& s = *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  JS::RootedObject record(cx, &js::GetFunctionNativeReserved(&args.callee(), 1).toObject());
  args.rval().setUndefined();
  JS::RootedValue generation(cx), step(cx);
  if (!JS_GetElement(cx, record, 0, &generation) || !JS_GetElement(cx, record, 1, &step)) return false;
  if (uint32_t(generation.toInt32()) != s.generation) return true;
  JSAutoRealm realm(cx, s.kit.page);
  if (s.paused && step.toInt32() != kCancelEnd) { s.heldStep = step.toInt32(); return true; }
  return Step(s, step.toInt32());
}
bool Post(State& s, int32_t step, double delay) {
  JSContext* cx = s.cx;
  if (!s.taskPort) return true;
  JS::RootedObject record(cx, JS::NewArrayObject(cx, 2));
  if (!record || !JS_SetElement(cx, record, 0, int32_t(s.generation)) ||
      !JS_SetElement(cx, record, 1, step)) return false;
  JSFunction* function = js::NewFunctionWithReserved(cx, RunStep, 0, 0, "SpeechSynthesis step");
  if (!function) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(&s));
  js::SetFunctionNativeReserved(task, 1, JS::ObjectValue(*record));
  return s.taskPort(s.taskContext, task, delay);
}
}
bool SynthesisMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kSynthesisClass, kMethods[slot], &self)) return false;
  State& s = Owner(self);
  JSAutoRealm realm(cx, s.kit.page);
  args.rval().setUndefined();
  if (slot == 4) return Voices(s, args.rval()) && JS_WrapValue(cx, args.rval());
  if (slot == 0) {
    if (!args.length())
      return WebKit::Fail(cx, "SpeechSynthesis.speak: At least 1 argument required, but only 0 passed");
    JS::RootedObject utterance(cx);
    JSObject* unwrapped = args[0].isObject() ? js::CheckedUnwrapStatic(&args[0].toObject()) : nullptr;
    if (!unwrapped || JS::GetClass(unwrapped) != &kUtteranceClass)
      return WebKit::Fail(cx, "SpeechSynthesis.speak: Argument 1 does not implement interface SpeechSynthesisUtterance.");
    JS::RootedValue item(cx, JS::ObjectValue(*unwrapped));
    JS::RootedObject queue(cx, Queue(s));
    uint32_t length = 0;
    if (!JS_WrapValue(cx, &item) || !JS::GetArrayLength(cx, queue, &length) ||
        !JS_SetElement(cx, queue, length, item)) return false;
    return s.speaking || length || Post(s, kBegin);
  }
  if (slot == 1) {  // cancel: drop the queue; the current utterance ends.
    JS::RootedObject empty(cx, JS::NewArrayObject(cx, 0));
    if (!empty) return false;
    JS::SetReservedSlot(s.instance, SynthQueue, JS::ObjectValue(*empty));
    ++s.generation;
    s.paused = false;
    s.heldStep = kNoStep;
    return !s.speaking || Post(s, kCancelEnd);
  }
  const JS::Value current = JS::GetReservedSlot(s.instance, SynthCurrent);
  JS::RootedObject utterance(cx, current.isObject() ? &current.toObject() : nullptr);
  if (slot == 2) {  // pause
    if (s.paused) return true;
    s.paused = true;
    return !utterance || Fire(s, utterance, "pause", 0, JS::NullHandleValue, "");
  }
  if (!s.paused) return true;  // resume
  s.paused = false;
  const int32_t held = s.heldStep;
  s.heldStep = kNoStep;
  if (utterance && !Fire(s, utterance, "resume", 0, JS::NullHandleValue, "")) return false;
  return held == kNoStep || Post(s, held);
}
}  // namespace ruyijs::web_speech
