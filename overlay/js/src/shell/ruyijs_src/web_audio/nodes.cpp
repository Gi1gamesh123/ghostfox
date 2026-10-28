#include "web_audio/node_fields.h"

#include "js/Array.h"
#include "js/Object.h"
#include "js/String.h"
#include "runtime/native_constructor_trace.h"

namespace ruyijs::web_audio {
namespace {

using detail::Self;

// The GainNode spec is not in the imported domain table; it is the one node
// parameter the acceptance criteria name explicitly (D34).
constexpr ParamSpec kGainSpec = {1.0, -3.4028234663852886e38, 3.4028234663852886e38, true};

// The kNodeDefs row whose constructor is being used, or -1 when the new target
// is the base AudioNode (which has no table row).
int32_t DefIndexForNewTarget(State& state, const JS::CallArgs& args) {
  JSObject* target = &args.callee();
  const auto& constructors = state.nodeConstructors.get();
  for (uint32_t index = 0; index < constructors.length(); ++index) {
    if (index >= uint32_t(kNodeDefCount)) break;
    if (constructors[index] == target) return int32_t(index);
  }
  return -1;
}

void SetContext(JSContext* cx, JS::HandleObject node, const JS::CallArgs& args) {
  JS::Value context = JS::UndefinedValue();
  if (args.length() && args[0].isObject() &&
      JS::GetClass(&args[0].toObject()) == &kContextClass)
    context = args[0];
  JS::SetReservedSlot(node, kNodeContextSlot, context);
}

}  // namespace

bool AttachParams(State& state, JS::HandleObject node, int32_t kind) {
  ParamSpec specs[kNodeParamSlots];
  int32_t count = 0;
  if (kind == kOscillatorNode) {
    // The frequency range is context-local: +/- half the context's sample rate.
    JSObject* context = detail::ContextOf(node);
    const double rate = context && JS::GetReservedSlot(context, kContextRateSlot).isNumber()
                            ? JS::GetReservedSlot(context, kContextRateSlot).toNumber()
                            : state.model.sample_rate();
    specs[0] = Model::oscillator_frequency(rate);
    specs[1] = Model::oscillator_detune();
    count = 2;
  } else if (kind == kCompressorNode) {
    specs[0] = Model::compressor_threshold();
    specs[1] = Model::compressor_knee();
    specs[2] = Model::compressor_ratio();
    specs[3] = Model::compressor_attack();
    specs[4] = Model::compressor_release();
    count = 5;
  } else if (kind == kGainNode) {
    specs[0] = kGainSpec;
    count = 1;
  }
  for (int32_t index = 0; index < count; ++index) {
    JS::RootedObject param(state.cx, NewParam(state, specs[index]));
    if (!param) return false;
    JS::SetReservedSlot(node, kNodeParamBase + index, JS::ObjectValue(*param));
  }
  if (kind == kOscillatorNode) {
    JS::RootedString sine(state.cx, JS_NewStringCopyZ(state.cx, "sine"));
    if (!sine) return false;
    JS::SetReservedSlot(node, kNodeTypeSlot, JS::StringValue(sine));
  }
  return true;
}

bool RegisterSourceNode(State& state, JS::HandleObject node) {
  JSContext* cx = state.cx;
  JS::Value context = JS::GetReservedSlot(node, kNodeContextSlot);
  if (!context.isObject() ||
      JS::GetClass(&context.toObject()) != &kContextClass) return true;
  JS::RootedObject owner(cx, &context.toObject());
  JS::Value list = JS::GetReservedSlot(owner, kContextNodesSlot);
  if (!list.isObject()) return true;
  JS::RootedObject nodes(cx, &list.toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, nodes, &length)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*node));
  return JS_DefineElement(cx, nodes, length, value, JSPROP_ENUMERATE);
}

bool ChildGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t encoded = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!detail::NodeReceiver(cx, args, encoded / 8, &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, kNodeParamBase + encoded % 8));
  return true;
}

bool NodeCtor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = Self(args);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "Constructor requires 'new'");
  const int32_t index = DefIndexForNewTarget(*state, args);
  if (!detail::IsContext(cx, args.get(0))) return false;
  // MediaStreamTrack instances will come from the Media owner, never a duck type.
  if (index == 19)
    return WebKit::Fail(cx, "MediaStreamTrackAudioSourceNode requires a MediaStreamTrack");
  const int32_t kind = index >= 0 ? kNodeDefs[index].kind : kGenericNode;
  JS::RootedObject proto(cx);
  if (!state->kit.ConstructProto(args, state->node.prototype, &proto)) return false;
  JS::RootedObject node(cx, state->kit.New(&kNodeClass, proto));
  if (!node) return false;
  JS::SetReservedSlot(node, kNodeKindSlot, JS::Int32Value(kind));
  JS::SetReservedSlot(node, kNodeDefIndexSlot, JS::Int32Value(index));
  SetContext(cx, node, args);
  if (!InitNodeChannels(*state, node, kind) || !AttachParams(*state, node, kind))
    return false;
  if (!InitNodeFields(*state, node, args.get(1))) return false;
  args.rval().setObject(*node);
  return trace.Finish(true);
}

bool MakeNode(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = Self(args);
  if (!args.thisv().isObject() ||
      JS::GetClass(&args.thisv().toObject()) != &kContextClass)
    return WebKit::Fail(cx, "Web Audio member called on an incompatible receiver");
  const int32_t slot = WebKit::Slot(args);
  if (slot == kSlotAudioBuffer) return CreateBuffer(*state, args, args.rval());
  if (slot == kSlotPeriodicWave) {
    JS::RootedObject object(cx, state->kit.New(nullptr, state->periodic.prototype));
    if (!object) return false;
    args.rval().setObject(*object);
    return true;
  }
  int32_t kind = kGenericNode;
  int32_t index = -1;
  JS::RootedObject proto(cx);
  if (slot == kSlotOscillator) {
    proto.set(state->oscillator.prototype.get());
    kind = kOscillatorNode;
  } else {
    proto.set(state->nodePrototypes.get()[slot]);
    kind = kNodeDefs[slot].kind;
    index = slot;
  }
  JS::RootedObject node(cx, state->kit.New(&kNodeClass, proto));
  if (!node) return false;
  JS::SetReservedSlot(node, kNodeKindSlot, JS::Int32Value(kind));
  JS::SetReservedSlot(node, kNodeDefIndexSlot, JS::Int32Value(index));
  JS::SetReservedSlot(node, kNodeContextSlot, args.thisv());
  if (!InitNodeChannels(*state, node, kind) || !AttachParams(*state, node, kind))
    return false;
  if (!InitNodeFields(*state, node, JS::UndefinedHandleValue)) return false;
  if (kind == kOscillatorNode && !RegisterSourceNode(*state, node)) return false;
  args.rval().setObject(*node);
  return true;
}

// AudioDestinationNode.maxChannelCount: the channel ceiling of the context
// that owns this destination (an AudioContext is stereo; an offline context
// reports the number of channels it was built with).
bool DestinationGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!detail::NodeReceiver(cx, args, kDestinationNode, &self)) return false;
  JSObject* context = detail::ContextOf(self);
  args.rval().setInt32(DestinationMaxChannels(
      context, context ? JS::GetReservedSlot(context, kContextChannelsSlot).toInt32() : 2));
  return true;
}

bool NodeMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!detail::NodeReceiver(cx, args, WebKit::Slot(args), &self)) return false;
  const int32_t def=JS::GetReservedSlot(self,kNodeDefIndexSlot).toInt32();
  if(JS::GetReservedSlot(self,kNodeKindSlot).toInt32()!=kOscillatorNode && def!=13 && def!=14)
    return WebKit::Fail(cx,"AudioScheduledSourceNode requires a source receiver");
  args.rval().setUndefined();
  return true;
}

}  // namespace ruyijs::web_audio
