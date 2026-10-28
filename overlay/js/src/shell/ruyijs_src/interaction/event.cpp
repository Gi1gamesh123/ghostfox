#include "interaction/internal.h"
#include "interaction/event_creation.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/GlobalObject.h"
#include "js/PropertyAndElement.h"
#include "js/Object.h"
namespace ruyijs::interaction { bool GetTrusted(JSContext*, unsigned, JS::Value*); }

namespace ruyijs::interaction {
static bool Method(JSContext* cx, unsigned argc, JS::Value* vp, unsigned kind) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args);
  if (!event) return false;
  static const char* const names[] = {"preventDefault", "stopPropagation",
                                      "stopImmediatePropagation"};
  ReportEventAccess(cx, event, "Event", names[kind], int(ApiAccess::Call));
  if (kind == 0) event->value.prevent_default();
  if (kind == 1) event->value.stop_propagation();
  if (kind == 2) event->value.stop_immediate_propagation();
  args.rval().setUndefined();
  return true;
}
static bool Prevent(JSContext* cx, unsigned n, JS::Value* vp) { return Method(cx, n, vp, 0); }
static bool Stop(JSContext* cx, unsigned n, JS::Value* vp) { return Method(cx, n, vp, 1); }
static bool StopImmediate(JSContext* cx, unsigned n, JS::Value* vp) { return Method(cx, n, vp, 2); }
static bool Path(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args);
  if (!event) return false;
  ReportEventAccess(cx, event, "Event", "composedPath", int(ApiAccess::Call));
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  if (!array) return false;
  if (!ComposeEventPath(cx, *event, array)) return false;
  args.rval().setObject(*array); return true;
}
static bool Init(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args);
  if (!event) return false;
  ReportEventAccess(cx, event, "Event", "initEvent", int(ApiAccess::Call));
  if (argc == 0) return Error(cx, "TypeError", "initEvent requires a type");
  JS::RootedString type(cx);
  std::string key;
  if (!Text(cx, args[0], &type, &key)) return false;
  if (!event->value.dispatching()) {
    event->type = type;
    event->value.initialize(domain::EventType(std::move(key)),
        JS::ToBoolean(args.get(1)), JS::ToBoolean(args.get(2)));
  }
  args.rval().setUndefined(); return true;
}
bool InstallTrusted(State& state, JS::HandleObject object) {
  const auto kind = static_cast<Kind>(JS::GetReservedSlot(object, kEventKind).toInt32());
  // CookieChangeEvent keeps the ordinary trusted getter: a jar write marks the
  // event trusted and a script-constructed one reports false, as Firefox does.
  if (kind == Kind::NavigationEntryChange || kind == Kind::MidiConnection ||
      kind == Kind::MidiMessage || kind == Kind::GpuUncaptured ||
      kind == Kind::IdbVersionChange) return true;
  if (kind == Kind::AudioProcessing || kind == Kind::FontLoad || kind == Kind::TaskPriority)
    return InstallProcessingTrusted(state, object, kind);
  if (kind == Kind::PictureInPicture) return InstallPictureInPictureTrusted(state, object);
  if (kind == Kind::Navigate) return InstallNavigateTrusted(state, object);
  if (kind == Kind::RtcTone || kind == Kind::RtcErrorPayload || kind == Kind::RtcIceError)
    return InstallRtcExtraTrusted(state, object, kind);
  if (kind == Kind::MediaEncrypted || kind == Kind::MediaKeyMessage ||
      kind == Kind::MediaStream || kind == Kind::MediaTrack)
    return InstallMediaEventTrusted(state, object, kind);
  if (kind == Kind::Speech || kind == Kind::SpeechError)
    return InstallSpeechTrusted(state, object, kind);
  JS::RootedObject setter(state.cx);
  return JS_DefineProperty(state.cx, object, "isTrusted", state.trustedGetter, setter,
                           JSPROP_ENUMERATE | JSPROP_PERMANENT);
}
bool EventConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing() || argc == 0)
    return Error(cx, "TypeError", "Event requires new and a type");
  State* state = activeState;
  while (state && state->page != JS::CurrentGlobalOrNull(cx)) state = state->previous;
  if (!state) return Error(cx, "TypeError", "Event realm is not installed");
  state->Observe("Event", "constructor", int(ApiAccess::Construct));
  if (state->events.size() >= 100000) return Error(cx, "RangeError", "Event limit exceeded");
  JS::RootedString type(cx);
  std::string key;
  if (!Text(cx, args[0], &type, &key)) return false;
  bool bubbles = false, cancelable = false, composed = false;
  // One base init path shared with every derived event constructor.
  if (!ReadEventBase(cx, args.get(1), &bubbles, &cancelable, &composed)) return false;
  JS::RootedObject object(cx, JS_NewObjectForConstructor(cx, &kEventClass, args));
  if (!object) return false;
  JS::SetReservedSlot(object, kEventKind,
                      JS::Int32Value(static_cast<int32_t>(Kind::Event)));
  if (!InstallTrusted(*state, object)) return false;
  auto event = std::make_unique<Event>(cx, object, type, std::move(key), bubbles, cancelable);
  event->value.set_composed(composed);
  event->value.set_time_stamp(std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - state->started).count());
  state->events.push_back(std::move(event));
  args.rval().setObject(*object); return trace.Finish(true);
}
bool InstallEvent(State& state) {
  static const JSFunctionSpec methods[] = {
    JS_FN("preventDefault", Prevent, 0, JSPROP_ENUMERATE),
    JS_FN("stopPropagation", Stop, 0, JSPROP_ENUMERATE),
    JS_FN("stopImmediatePropagation", StopImmediate, 0, JSPROP_ENUMERATE),
    JS_FN("composedPath", Path, 0, JSPROP_ENUMERATE),
    JS_FN("initEvent", Init, 1, JSPROP_ENUMERATE), JS_FS_END
  };
  JS::RootedObject prototype(state.cx, JS_InitClass(state.cx, state.page, &kEventClass,
      nullptr, "Event", EventConstructor, 1, kEventProperties, methods, nullptr, nullptr));
  if (!prototype || !Tag(state.cx, prototype, "Event")) return false;
  state.eventPrototype = prototype;
  JSFunction* getter = JS_NewFunction(state.cx, GetTrusted, 0, 0, "get isTrusted");
  if (!getter) return false;
  state.trustedGetter = JS_GetFunctionObject(getter);
  JS::RootedValue constructor(state.cx);
  if (!JS_GetProperty(state.cx, state.page, "Event", &constructor)) return false;
  JS::RootedObject function(state.cx, &constructor.toObject());
  const char* names[] = {"NONE", "CAPTURING_PHASE", "AT_TARGET", "BUBBLING_PHASE",
                         "ALT_MASK", "CONTROL_MASK", "SHIFT_MASK", "META_MASK"};
  const int values[] = {0, 1, 2, 3, 1, 2, 4, 8};
  for (unsigned n = 0; n < 8; ++n) {
    JS::RootedValue value(state.cx, JS::Int32Value(values[n]));
    unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
    // The prototype copy is made permanent by the binding order pass, after it is placed last.
    if (!JS_DefineProperty(state.cx, prototype, names[n], value, flags & ~JSPROP_PERMANENT) ||
        !JS_DefineProperty(state.cx, function, names[n], value, flags)) return false;
  }
  return SaveLegacyPrototype(state, Iface::Event, prototype) &&
         state.helpers->Interface("Event", prototype) &&
         state.helpers->Member(state.trustedGetter, "Event", "get isTrusted", "get") &&
         state.helpers->Constructor("Event", function);
}
}
