#include "web_audio/node_fields.h"

#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "runtime/native_constructor_trace.h"

namespace ruyijs::web_audio {
using detail::IsContext;
using detail::Self;

namespace {

const char* StateText(int32_t state) {
  switch (state) {
    case kSuspendedState: return "suspended";
    case kClosedState: return "closed";
    default: return "running";
  }
}

// The state machine edge the caller asked for: 0 resume, 1 suspend, 2 close.
int32_t TargetState(int32_t slot) {
  if (slot == 0) return kRunningState;
  if (slot == 1) return kSuspendedState;
  return kClosedState;
}

}  // namespace

// AudioContext and OfflineAudioContext share this constructor and the donor's
// single "context" identity for BaseAudioContext factory receivers. The offline
// form keeps its (channels, length, sampleRate) so startRendering renders a
// deterministic silent buffer of those dimensions.
bool ContextCtor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = Self(args);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing())
    return WebKit::Fail(cx, "Constructor AudioContext requires 'new'");
  JS::RootedObject proto(cx);
  if (!state->kit.ConstructProto(args, state->base.prototype, &proto)) return false;
  JS::RootedObject object(cx, state->kit.New(&kContextClass, proto));
  if (!object) return false;
  // The called constructor determines the context kind even for subclasses.
  const bool offline = &args.callee() == state->offline.constructor.get();
  JS::SetReservedSlot(object, kContextOfflineSlot, JS::BooleanValue(offline));
  int32_t channels = 2, length = 0;
  double rate = state->model.sample_rate();
  if (!offline && CurrentProfile().hasSampleRate) rate = CurrentProfile().sampleRate;
  if (offline) {
    double rawChannels = 1, rawLength = 0, rawRate = 44100;
    if ((args.length() > 0 && !JS::ToNumber(cx, args[0], &rawChannels)) ||
        (args.length() > 1 && !JS::ToNumber(cx, args[1], &rawLength)) ||
        (args.length() > 2 && !JS::ToNumber(cx, args[2], &rawRate))) return false;
    if (!(rawChannels >= 1 && rawChannels <= 32) || !(rawLength >= 0) ||
        !(rawRate >= 8000 && rawRate <= 96000))
      return ThrowDOMException(*state, "NotSupportedError",
                               "OfflineAudioContext dimensions are out of range");
    channels = int32_t(rawChannels);
    length = int32_t(rawLength);
    rate = rawRate;
  }
  // Firefox's autoplay policy starts a live AudioContext suspended until the
  // page has been activated; this runtime tracks no activation, so the live
  // context reports "suspended" like a page without user activation. An offline
  // context renders as soon as startRendering runs.
  JS::SetReservedSlot(object, kContextStateSlot,
                      JS::Int32Value(offline ? kRunningState : kSuspendedState));
  JS::SetReservedSlot(object, kContextChannelsSlot, JS::Int32Value(channels));
  JS::SetReservedSlot(object, kContextLengthSlot, JS::Int32Value(length));
  JS::SetReservedSlot(object, kContextRateSlot, JS::DoubleValue(rate));
  if (!state->events->Attach(object)) return false;
  // One destination node and one listener per context, created here so the
  // BaseAudioContext accessors return stable instances rather than null.
  JS::RootedObject destination(cx,
      state->kit.New(&kNodeClass, state->destination.prototype));
  JS::RootedObject listener(cx,
      state->kit.New(&kListenerClass, state->listener.prototype));
  if (!destination || !listener) return false;
  JS::SetReservedSlot(destination, kNodeKindSlot, JS::Int32Value(kDestinationNode));
  JS::SetReservedSlot(destination, kNodeDefIndexSlot, JS::Int32Value(-1));
  JS::SetReservedSlot(destination, kNodeContextSlot, JS::ObjectValue(*object));
  if (!InitNodeChannels(*state, destination, kDestinationNode) ||
      !InitNodeFields(*state, destination, JS::UndefinedHandleValue) ||
      !InitListener(*state, listener)) return false;
  // One AudioWorklet per context; SameObject for this context's whole lifetime.
  JS::RootedObject worklet(cx, state->kit.New(&kWorkletClass, state->audioWorklet.prototype));
  if (!worklet) return false;
  JS::SetReservedSlot(object, kContextDestinationSlot, JS::ObjectValue(*destination));
  JS::SetReservedSlot(object, kContextListenerSlot, JS::ObjectValue(*listener));
  JS::SetReservedSlot(object, kContextWorkletSlot, JS::ObjectValue(*worklet));
  // The source-node list startRendering walks, and the oncomplete handler's
  // initial null (a non-callable assignment stores null too).
  JS::RootedObject nodes(cx, JS::NewArrayObject(cx, 0));
  if (!nodes) return false;
  JS::SetReservedSlot(object, kContextNodesSlot, JS::ObjectValue(*nodes));
  JS::SetReservedSlot(object, kContextOnCompleteSlot, JS::NullValue());
  args.rval().setObject(*object);
  return trace.Finish(true);
}

bool ContextGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = Self(args);
  if (!IsContext(cx, args.thisv())) return false;
  JS::RootedObject self(cx, &args.thisv().toObject());
  const int32_t member = WebKit::Slot(args);
  if ((member == 5 && !detail::IsContextKind(cx, args.thisv(), true)) ||
      ((member == 6 || member == 7) && !detail::IsContextKind(cx, args.thisv(), false)))
    return false;
  switch (member) {
    case 0:
      args.rval().setNumber(state->Now());
      break;
    case 1:
      args.rval().set(JS::GetReservedSlot(self, kContextRateSlot));
      break;
    case 2: {
      JS::RootedString text(cx, JS_NewStringCopyZ(
          cx, StateText(JS::GetReservedSlot(self, kContextStateSlot).toInt32())));
      if (!text) return false;
      args.rval().setString(text);
      break;
    }
    case 3:
      args.rval().set(JS::GetReservedSlot(self, kContextDestinationSlot));
      break;
    case 4:
      args.rval().set(JS::GetReservedSlot(self, kContextListenerSlot));
      break;
    case 5:
      args.rval().set(JS::GetReservedSlot(self, kContextLengthSlot));
      break;
    case 6:
      args.rval().setNumber(CurrentProfile().hasBaseLatency ? CurrentProfile().baseLatency
                                                            : kBaseLatency);
      break;
    case 7:
      args.rval().setNumber(
          OutputLatencyFor(JS::GetReservedSlot(self, kContextStateSlot).toInt32()));
      break;
    case 8:
      args.rval().set(JS::GetReservedSlot(self, kContextWorkletSlot));
      break;
    case kOnCompleteMember:
      args.rval().set(JS::GetReservedSlot(self, kContextOnCompleteSlot));
      break;
    default:
      args.rval().setNull();
      break;
  }
  return true;
}

// resume/suspend/close run the one state machine. A closed context rejects with
// InvalidStateError; otherwise the transition (when it changes state) fires
// statechange on the interaction tree and the promise resolves.
bool ContextPromise(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = Self(args);
  if (!IsContext(cx, args.thisv())) return false;
  JS::RootedObject self(cx, &args.thisv().toObject());
  const int32_t operation = WebKit::Slot(args);
  if (operation && !detail::IsContextKind(cx, args.thisv(), false)) return false;
  if (!operation && JS::GetReservedSlot(self, kContextOfflineSlot).toBoolean()) {
    JS::RootedValue reason(cx);
    if (!MakeDOMException(*state, "NotSupportedError",
                          "OfflineAudioContext does not support resume", &reason)) return false;
    return state->kit.RejectWith(reason, args.rval());
  }
  const int32_t current = JS::GetReservedSlot(self, kContextStateSlot).toInt32();
  if (current == kClosedState) {
    JS::RootedValue reason(cx);
    if (!MakeDOMException(*state, "InvalidStateError",
                          "The AudioContext is closed", &reason)) return false;
    return state->kit.RejectWith(reason, args.rval());
  }
  const int32_t next = TargetState(WebKit::Slot(args));
  if (next != current) {
    JS::SetReservedSlot(self, kContextStateSlot, JS::Int32Value(next));
    if (!state->events->Fire(self, "statechange", false, false)) return false;
  }
  return state->kit.Resolve(JS::UndefinedHandleValue, args.rval());
}

bool ResolvedPromise(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = Self(args);
  JS::RootedObject self(cx);
  // A Promise-returning operation reports its brand and argument-count errors
  // as a rejected promise (WebIDL), as Firefox's Worklet.addModule does.
  const bool ok = WebKit::Check(cx, args.thisv(), &kWorkletClass, &self,
                                "'addModule' called on an object that does not implement interface Worklet.") &&
                  (args.length() || WebKit::Fail(cx, "Worklet.addModule: At least 1 argument required, but only 0 passed"));
  if (ok) return state->kit.Resolve(JS::UndefinedHandleValue, args.rval());
  JS::RootedValue error(cx);
  if (!JS_GetPendingException(cx, &error)) return false;
  JS_ClearPendingException(cx);
  return state->kit.RejectWith(error, args.rval());
}


}  // namespace ruyijs::web_audio
