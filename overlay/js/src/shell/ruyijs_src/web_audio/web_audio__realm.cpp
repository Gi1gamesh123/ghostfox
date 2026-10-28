#include "web_audio/state.h"

#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/String.h"
#include "js/ValueArray.h"
#include "js/Wrapper.h"

namespace ruyijs::web_audio {

// AudioContext and OfflineAudioContext instances share one class, like the
// donor's single "context" brand; BaseAudioContext is never instantiated.
const JSClass kContextClass = {"AudioContext",
                               JSCLASS_HAS_RESERVED_SLOTS(kContextSlotCount)};
const JSClass kNodeClass = {"AudioNode", JSCLASS_HAS_RESERVED_SLOTS(kNodeSlotCount)};
const JSClass kParamClass = {"AudioParam", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kBufferClass = {"AudioBuffer",
                              JSCLASS_HAS_RESERVED_SLOTS(kBufferSlotCount)};
const JSClass kListenerClass = {"AudioListener",
                                JSCLASS_HAS_RESERVED_SLOTS(kListenerSlotCount)};
const JSClass kWorkletClass = {"AudioWorklet", 0};
const JSClass kWorkletBaseClass = {"Worklet", 0};
bool IsAudioContextValue(JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  return object && JS::GetClass(object) == &kContextClass;
}

State::State(JSContext* context, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events_)
    : cx(context),
      kit(context, page, helpers, this),
      model(),
      events(&events_),
      base(context),
      context(context),
      offline(context),
      node(context),
      scheduled(context),
      oscillator(context),
      compressor(context),
      param(context),
      buffer(context),
      periodic(context),
      listener(context),
      worklet(context),
      audioWorklet(context),
      destination(context),
      paramMap(context),
      nodePrototypes(context, JS::GCVector<JSObject*>(context)),
      nodeConstructors(context, JS::GCVector<JSObject*>(context)) {}

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events)
    : state_(std::make_unique<State>(cx, page, helpers, events)) {}
Realm::~Realm() = default;

void Realm::SetTaskPort(TaskPort port, void* context) {
  state_->taskPort = port;
  state_->taskContext = context;
}
void Realm::SetChannelPort(ChannelPort port, void* context) {
  state_->channelPort = port;
  state_->channelContext = context;
}
void Realm::SetClockPort(ClockPort clock, void* context) {
  state_->clock = clock;
  state_->clockContext = context;
}

// The one error value factory for this domain: the object comes from the page
// realm's DOMException interface, so Web Audio owns no second error type.
bool MakeDOMException(State& state, const char* name, const char* message,
                      JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  JS::RootedValue constructor(cx);
  if (!JS_GetProperty(cx, state.kit.page, "DOMException", &constructor) ||
      !constructor.isObject())
    return WebKit::Fail(cx, "Web Audio requires the page DOMException");
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message));
  JS::RootedString label(cx, JS_NewStringCopyZ(cx, name));
  if (!text || !label) return false;
  JS::RootedValueArray<2> arguments(cx);
  arguments[0].setString(text);
  arguments[1].setString(label);
  JS::RootedObject error(cx);
  if (!JS::Construct(cx, constructor, arguments, &error)) return false;
  result.setObject(*error);
  return true;
}

bool ThrowDOMException(State& state, const char* name, const char* message) {
  JS::RootedValue error(state.cx);
  if (!MakeDOMException(state, name, message, &error)) return false;
  JS_SetPendingException(state.cx, error);
  return false;
}

}  // namespace ruyijs::web_audio
