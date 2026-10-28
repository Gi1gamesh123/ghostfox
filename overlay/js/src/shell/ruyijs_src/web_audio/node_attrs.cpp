#include "web_audio/internal.h"

#include <algorithm>

#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

// The AudioNode attribute surface: identity (context, input/output counts) and
// the channel mixing state. Firefox validates channelCount in [1, 32] and
// rejects an AudioDestinationNode count above maxChannelCount; the mode and
// interpretation are the ChannelCountMode/ChannelInterpretation enumerations.
namespace ruyijs::web_audio {
namespace {

using detail::ContextOf;
using detail::NodeReceiver;

// Input/output counts come from the node's kNodeDefs row; -1 (the base
// AudioNode and the oscillator, which ruyijs builds outside the table) falls back
// to the kind.
void CountsOf(JSObject* node, uint32_t* inputs, uint32_t* outputs) {
  const int32_t index = JS::GetReservedSlot(node, kNodeDefIndexSlot).toInt32();
  if (index >= 0 && index < kNodeDefCount) {
    *inputs = kNodeDefs[index].inputs;
    *outputs = kNodeDefs[index].outputs;
    return;
  }
  const int32_t kind = JS::GetReservedSlot(node, kNodeKindSlot).toInt32();
  if (kind == kOscillatorNode) {
    *inputs = 0;
    *outputs = 1;
    return;
  }
  if (kind == kDestinationNode) {
    *inputs = 1;
    *outputs = 0;
    return;
  }
  *inputs = 1;
  *outputs = 1;
}

bool StoreString(JSContext* cx, JS::HandleObject node, int32_t slot,
                 JS::HandleString text) {
  JS::SetReservedSlot(node, slot, JS::StringValue(text));
  return true;
}

// One enum-valued setter: the text must be one of the two/three allowed names.
bool StoreEnum(JSContext* cx, const JS::CallArgs& args, JS::HandleObject node,
               int32_t slot, const char* first, const char* second,
               const char* third, const char* message) {
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  if (!text) return false;
  bool match = false;
  bool firstMatch = false, secondMatch = false, thirdMatch = false;
  if (!JS_StringEqualsAscii(cx, text, first, &firstMatch) ||
      !JS_StringEqualsAscii(cx, text, second, &secondMatch) ||
      (third && !JS_StringEqualsAscii(cx, text, third, &thirdMatch))) return false;
  match = firstMatch || secondMatch || (third && thirdMatch);
  if (!match) return WebKit::Fail(cx, message);
  return StoreString(cx, node, slot, text) && (args.rval().setUndefined(), true);
}

}  // namespace

// Installs the defaults Firefox reports before any setter runs: channelCount 2
// (the context's channel count for a destination), "max" and "speakers". An
// AudioDestinationNode is the one node whose mode is "explicit"
// (AudioDestinationNode::ChannelCountMode returns Explicit).
bool InitNodeChannels(State& state, JS::HandleObject node, int32_t kind) {
  JSContext* cx = state.cx;
  int32_t count = 2;
  if (kind == kDestinationNode) {
    JSObject* context = ContextOf(node);
    count = context ? JS::GetReservedSlot(context, kContextChannelsSlot).toInt32() : 2;
  }
  JS::SetReservedSlot(node, kNodeChannelCountSlot, JS::Int32Value(count));
  JS::RootedString mode(
      cx, JS_NewStringCopyZ(cx, kind == kDestinationNode ? "explicit" : "max"));
  JS::RootedString interpretation(cx, JS_NewStringCopyZ(cx, "speakers"));
  if (!mode || !interpretation) return false;
  return StoreString(cx, node, kNodeChannelModeSlot, mode) &&
         StoreString(cx, node, kNodeInterpretationSlot, interpretation);
}

bool NodeAttributeGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!NodeReceiver(cx, args, kGenericNode, &self)) return false;
  switch (WebKit::Slot(args)) {
    case kAttrNumberOfInputs:
    case kAttrNumberOfOutputs: {
      uint32_t inputs = 0, outputs = 0;
      CountsOf(self, &inputs, &outputs);
      args.rval().setInt32(int32_t(WebKit::Slot(args) == kAttrNumberOfInputs ? inputs
                                                                           : outputs));
      break;
    }
    case kAttrContext: {
      JSObject* context = ContextOf(self);
      if (context) args.rval().setObject(*context);
      else args.rval().setUndefined();
      break;
    }
    case kAttrChannelCount:
      args.rval().set(JS::GetReservedSlot(self, kNodeChannelCountSlot));
      break;
    case kAttrChannelCountMode:
      args.rval().set(JS::GetReservedSlot(self, kNodeChannelModeSlot));
      break;
    default:
      args.rval().set(JS::GetReservedSlot(self, kNodeInterpretationSlot));
      break;
  }
  return true;
}

bool NodeAttributeSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = detail::Self(args);
  JS::RootedObject self(cx);
  if (!NodeReceiver(cx, args, kGenericNode, &self)) return false;
  const int32_t slot = WebKit::Slot(args);
  if (slot == kAttrChannelCount) {
    double raw = 0;
    if (!JS::ToNumber(cx, args.get(0), &raw)) return false;
    const int32_t count = int32_t(raw);
    if (JS::GetReservedSlot(self, kNodeKindSlot).toInt32() == kDestinationNode) {
      const JSObject* context = ContextOf(self);
      const int32_t max = DestinationMaxChannels(
          const_cast<JSObject*>(context),
          context ? JS::GetReservedSlot(context, kContextChannelsSlot).toInt32() : 2);
      if (count > max)
        return ThrowDOMException(*state, "IndexSizeError",
                                 "channelCount is larger than maxChannelCount");
    } else if (count < 1 || count > int32_t(kMaxChannelCount)) {
      return ThrowDOMException(
          *state, "NotSupportedError",
          "Channel count must be in the range [1, max supported channel count]");
    }
    JS::SetReservedSlot(self, kNodeChannelCountSlot, JS::Int32Value(count));
    args.rval().setUndefined();
    return true;
  }
  if (slot == kAttrChannelCountMode)
    return StoreEnum(cx, args, self, kNodeChannelModeSlot, "max", "clamped-max",
                     "explicit", "channelCountMode must be max, clamped-max or explicit");
  return StoreEnum(cx, args, self, kNodeInterpretationSlot, "speakers", "discrete",
                   nullptr,
                   "channelInterpretation must be speakers or discrete");
}

// AudioContext.getOutputTimestamp(): the rendered position is the current time
// minus the output latency, and the performance timestamp comes from the same
// clock (A13.4 guarantees currentTime*1000 === performance.now()).
bool ContextTimestamp(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = detail::Self(args);
  if (!detail::IsContextKind(cx, args.thisv(), false)) return false;
  const double now = state->Now();
  const double latency = OutputLatencyFor(
      JS::GetReservedSlot(&args.thisv().toObject(), kContextStateSlot).toInt32());
  JS::RootedObject stamp(cx, JS_NewPlainObject(cx));
  if (!stamp) return false;
  JS::RootedValue contextTime(cx, JS::NumberValue(std::max(0.0, now - latency)));
  JS::RootedValue performanceTime(
      cx, JS::NumberValue(std::max(0.0, now * 1000.0 - latency * 1000.0)));
  if (!JS_DefineProperty(cx, stamp, "contextTime", contextTime, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, stamp, "performanceTime", performanceTime,
                         JSPROP_ENUMERATE)) return false;
  args.rval().setObject(*stamp);
  return true;
}

}  // namespace ruyijs::web_audio
