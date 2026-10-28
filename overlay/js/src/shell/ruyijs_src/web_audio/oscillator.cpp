#include "web_audio/node_fields.h"

#include "js/Conversions.h"
#include "js/Object.h"
#include "js/String.h"
#include "runtime/native_constructor_trace.h"

namespace ruyijs::web_audio {
namespace {

using detail::Self;

constexpr const char* kTypes[] = {"sine", "square", "sawtooth", "triangle", "custom"};

bool IsStandardType(JSContext* cx, JS::HandleString text, bool* matched) {
  for (const char* candidate : kTypes)
    if (!JS_StringEqualsAscii(cx, text, candidate, matched)) return false;
    else if (*matched) return true;
  *matched = false;
  return true;
}

}  // namespace

bool OscillatorCtor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = Self(args);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing())
    return WebKit::Fail(cx, "Constructor OscillatorNode requires 'new'");
  if (args.length() < 1 || !args[0].isObject() ||
      JS::GetClass(&args[0].toObject()) != &kContextClass)
    return WebKit::Fail(cx, "AudioContext required");
  JS::RootedObject proto(cx);
  if (!state->kit.ConstructProto(args, state->scheduled.prototype, &proto)) return false;
  JS::RootedObject node(cx, state->kit.New(&kNodeClass, proto));
  if (!node) return false;
  JS::SetReservedSlot(node, kNodeKindSlot, JS::Int32Value(kOscillatorNode));
  JS::SetReservedSlot(node, kNodeDefIndexSlot, JS::Int32Value(-1));
  JS::SetReservedSlot(node, kNodeContextSlot, args[0]);
  if (!InitNodeChannels(*state, node, kOscillatorNode) ||
      !AttachParams(*state, node, kOscillatorNode) ||
      !InitNodeFields(*state, node, args.get(1)) ||
      !RegisterSourceNode(*state, node)) return false;
  args.rval().setObject(*node);
  return trace.Finish(true);
}

// OscillatorNode.type: the four standard waveforms plus "custom". The donor had
// no type member; this is a spec superset recorded in D34.
bool OscillatorTypeGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!detail::NodeReceiver(cx, args, kOscillatorNode, &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, kNodeTypeSlot));
  return true;
}

bool OscillatorTypeSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!detail::NodeReceiver(cx, args, kOscillatorNode, &self)) return false;
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  bool matched = false;
  if (!text || !IsStandardType(cx, text, &matched)) return false;
  if (!matched)
    return WebKit::Fail(cx, "OscillatorNode.type must be a standard waveform");
  JS::SetReservedSlot(self, kNodeTypeSlot, JS::StringValue(text));
  args.rval().setUndefined();
  return true;
}

}  // namespace ruyijs::web_audio
