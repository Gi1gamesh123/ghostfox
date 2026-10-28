// GPUCommandEncoder, GPURenderPassEncoder and GPUCommandBuffer: the encoding
// state machine from createCommandEncoder through beginRenderPass, end, finish
// and queue.submit. The commands encode nothing; the observable contract is the
// transition rules and the thrown OperationError on a repeated finish.
#include "web_gpu/internal.h"

#include "js/Realm.h"

namespace ruyijs::web_gpu {

JSObject* NewCommandEncoder(State& state, JS::HandleString label) {
  JSContext* cx = state.cx;
  const JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject encoder(cx, state.kit.New(&kEncoderClass, state.encoder.prototype));
  if (!encoder) return nullptr;
  JS::SetReservedSlot(encoder, kEncoderFinished, JS::BooleanValue(false));
  JS::SetReservedSlot(encoder, kEncoderPassActive, JS::BooleanValue(false));
  JS::SetReservedSlot(encoder, kEncoderLabel, JS::StringValue(label));
  return encoder.get();
}

JSObject* NewRenderPass(State& state, JS::HandleObject encoder, JS::HandleString label) {
  JSContext* cx = state.cx;
  const JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject pass(cx, state.kit.New(&kPassClass, state.pass.prototype));
  if (!pass) return nullptr;
  JS::SetReservedSlot(pass, kPassEncoder, JS::ObjectValue(*encoder));
  JS::SetReservedSlot(pass, kPassEnded, JS::BooleanValue(false));
  JS::SetReservedSlot(pass, kPassLabel, JS::StringValue(label));
  return pass.get();
}

JSObject* NewCommandBuffer(State& state, JS::HandleString label) {
  JSContext* cx = state.cx;
  const JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject buffer(cx,
      state.kit.New(&kCommandBufferClass, state.command_buffer.prototype));
  if (!buffer) return nullptr;
  JS::SetReservedSlot(buffer, kCommandBufferLabel, JS::StringValue(label));
  return buffer.get();
}

bool EncoderProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CheckEncoder(cx, args.thisv(), &self)) return false;
  std::string text;
  if (!SlotString(cx, self, kEncoderLabel, &text)) return false;
  return StringValue(cx, text, args.rval());
}

bool PassProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CheckPass(cx, args.thisv(), &self)) return false;
  std::string text;
  if (!SlotString(cx, self, kPassLabel, &text)) return false;
  return StringValue(cx, text, args.rval());
}

bool CommandBufferProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CheckCommandBuffer(cx, args.thisv(), &self)) return false;
  std::string text;
  if (!SlotString(cx, self, kCommandBufferLabel, &text)) return false;
  return StringValue(cx, text, args.rval());
}

bool EncoderMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!CheckEncoder(cx, args.thisv(), &self)) return false;
  switch (WebKit::Slot(args)) {
    case kEncoderBeginRenderPass: {
      if (JS::GetReservedSlot(self, kEncoderFinished).toBoolean())
        return ThrowOperation(*state, "the encoder is already finished");
      if (JS::GetReservedSlot(self, kEncoderPassActive).toBoolean())
        return ThrowOperation(*state, "a render pass is already open");
      JS::RootedValue descriptor(cx, args.get(0));
      if (!descriptor.isObject())
        return ThrowError(*state, "TypeError", nullptr,
                          "beginRenderPass requires a descriptor dictionary");
      JS::RootedObject dictionary(cx, &descriptor.toObject());
      JS::RootedString label(cx, JS_NewStringCopyZ(cx, ""));
      if (!label || !ReadLabel(cx, dictionary, &label)) return false;
      JS::RootedObject pass(cx, NewRenderPass(*state, self, label));
      if (!pass) return false;
      JS::SetReservedSlot(self, kEncoderPassActive, JS::BooleanValue(true));
      args.rval().setObject(*pass);
      return true;
    }
    default: {  // finish
      if (JS::GetReservedSlot(self, kEncoderFinished).toBoolean())
        return ThrowOperation(*state, "the encoder is already finished");
      if (JS::GetReservedSlot(self, kEncoderPassActive).toBoolean())
        return ThrowOperation(*state, "the render pass has not ended");
      JS::RootedString label(cx, JS_NewStringCopyZ(cx, ""));
      if (!label) return false;
      JS::RootedValue descriptor(cx, args.get(0));
      if (descriptor.isObject()) {
        JS::RootedObject dictionary(cx, &descriptor.toObject());
        if (!ReadLabel(cx, dictionary, &label)) return false;
      }
      JS::SetReservedSlot(self, kEncoderFinished, JS::BooleanValue(true));
      JS::RootedObject buffer(cx, NewCommandBuffer(*state, label));
      if (!buffer) return false;
      args.rval().setObject(*buffer);
      return true;
    }
  }
}

bool PassMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!CheckPass(cx, args.thisv(), &self)) return false;
  if (JS::GetReservedSlot(self, kPassEnded).toBoolean())
    return ThrowOperation(*state, "the render pass has already ended");
  JS::SetReservedSlot(self, kPassEnded, JS::BooleanValue(true));
  JS::RootedValue encoder(cx, JS::GetReservedSlot(self, kPassEncoder));
  if (encoder.isObject()) {
    JS::RootedObject owner(cx, &encoder.toObject());
    if (JS::GetClass(owner) == &kEncoderClass)
      JS::SetReservedSlot(owner, kEncoderPassActive, JS::BooleanValue(false));
  }
  args.rval().setUndefined();
  return true;
}

}  // namespace ruyijs::web_gpu
