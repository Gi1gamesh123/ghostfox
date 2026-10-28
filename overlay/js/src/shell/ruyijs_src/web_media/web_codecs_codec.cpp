// WebCodecs codec interfaces: AudioDecoder/AudioEncoder/VideoDecoder/VideoEncoder.
// The constructors and members reproduce Firefox 158's surface and state-machine
// errors; no actual encode/decode runs. Instances are EventTargets (their
// prototype's parent is the Interaction owner's EventTarget.prototype), so the
// ondequeue handler and addEventListener behave like the donor.
#include "web_media/web_codecs.h"
#include "web_media/audio_data.h"
#include "web_media/image_decoder.h"
#include "web_media/media_runtime.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include "js/CallAndConstruct.h"
namespace ruyijs::web_media {
const JSClass kCodecClass = {"WebCodecs codec", JSCLASS_HAS_RESERVED_SLOTS(3)};
namespace {
const char* const kNames[4] = {"AudioDecoder", "VideoDecoder", "AudioEncoder", "VideoEncoder"};
const char* const kEngines[4] = {"Decoder", "Decoder", "Encoder", "Encoder"};
const char* const kArgInterfaces[4] = {"EncodedAudioChunk", "EncodedVideoChunk", "AudioData", "VideoFrame"};

JSObject* CodecReceiver(JSContext* cx, JS::HandleValue self, CodecKind kind) {
  JSObject* object = self.isObject() ? js::CheckedUnwrapStatic(&self.toObject()) : nullptr;
  if (object && JS::GetClass(object) == &kCodecClass &&
      JS::GetReservedSlot(object, SlotKind).toInt32() == int(kind)) return object;
  return nullptr;
}
// A Promise-returning member rejects the WebIDL brand failure instead of throwing
// it (Firefox keeps the invalid-receiver TypeError inside the returned Promise).
bool RejectBrand(JSContext* cx, const JS::CallArgs& args, const char* iface,
                 JS::MutableHandleValue out) {
  std::string text;
  if (!WebKit::BrandText(cx, args, iface, &text)) return false;
  JS::RootedValue reason(cx);
  WebKit::Fail(cx, text.c_str());
  if (!JS_GetPendingException(cx, &reason)) return false;
  JS_ClearPendingException(cx);
  if (!JS_WrapValue(cx, &reason)) return false;
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !JS::RejectPromise(cx, promise, reason)) return false;
  out.setObject(*promise);
  return true;
}
}
bool CodecConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  const CodecKind kind = CodecKind(WebKit::Slot(args));
  if (!args.isConstructing())
    return trace.Finish(WebKit::Fail(cx, (std::string(kNames[kind]) + " constructor: 'new' is required").c_str()));
  WebFace* faces[4] = {&state.audioDecoder, &state.videoDecoder, &state.audioEncoder, &state.videoEncoder};
  JS::RootedObject prototype(cx);
  if (!state.kit.ConstructProto(args, faces[kind]->prototype, &prototype)) return false;
  if (!argc)
    return trace.Finish(WebKit::Fail(cx, (std::string(kNames[kind]) + " constructor: At least 1 argument required, but only 0 passed").c_str()));
  JS::RootedObject init(cx, args.get(0).isObject() ? &args.get(0).toObject() : nullptr);
  JS::RootedValue error(cx), output(cx);
  if (init && (!JS_GetProperty(cx, init, "error", &error) || !JS_GetProperty(cx, init, "output", &output))) return false;
  // Required members are validated in reverse declaration order: error first.
  if (error.isUndefined())
    return trace.Finish(WebKit::Fail(cx, (std::string(kNames[kind]) + " constructor: Missing required 'error' member of " + kNames[kind] + "Init.").c_str()));
  if (output.isUndefined())
    return trace.Finish(WebKit::Fail(cx, (std::string(kNames[kind]) + " constructor: Missing required 'output' member of " + kNames[kind] + "Init.").c_str()));
  JS::RootedObject object(cx, state.kit.New(&kCodecClass, prototype));
  if (!object || !state.events.Attach(object)) return false;
  JS::SetReservedSlot(object, SlotKind, JS::Int32Value(int(kind)));
  JS::SetReservedSlot(object, SlotState, JS::Int32Value(int(Unconfigured)));
  JS::SetReservedSlot(object, SlotOndequeue, JS::NullValue());
  args.rval().setObject(*object);
  return trace.Finish(true);
}
bool CodecMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  const int32_t slot = WebKit::Slot(args);
  const CodecKind kind = CodecKind(slot / 16);
  const int sub = slot % 16;
  JS::RootedObject self(cx, CodecReceiver(cx, args.thisv(), kind));
  if (sub == CFlush) {
    if (!self) return RejectBrand(cx, args, kNames[kind], args.rval());
    JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
    if (!promise) return false;
    if (JS::GetReservedSlot(self, SlotState).toInt32() != int(Configured)) {
      JS::RootedValue reason(cx);
      if (!CodecError(state, "InvalidStateError",
                      std::string(kNames[kind]) + ".flush: " + kEngines[kind] + " must be configured first", &reason) ||
          !JS::RejectPromise(cx, promise, reason)) return false;
    } else if (!JS::ResolvePromise(cx, promise, JS::UndefinedHandleValue)) {
      return false;
    }
    args.rval().setObject(*promise);
    return true;
  }
  if (!self) return WebKit::FailBrand(cx, args, kNames[kind]);
  const int current = JS::GetReservedSlot(self, SlotState).toInt32();
  if (sub == CConfigure) {
    if (!argc)
      return WebKit::Fail(cx, (std::string(kNames[kind]) + ".configure: At least 1 argument required, but only 0 passed").c_str());
    // A config without a valid codec string is a TypeError (Firefox: "Invalid codec string").
    JS::RootedObject config(cx, args.get(0).isObject() ? &args.get(0).toObject() : nullptr);
    JS::RootedValue codec(cx);
    if (config && !JS_GetProperty(cx, config, "codec", &codec)) return false;
    if (!codec.isString() || !JS_GetStringLength(codec.toString()))
      return WebKit::Fail(cx, (std::string(kNames[kind]) + ".configure: Invalid codec string").c_str());
    if (current == int(Closed)) return MediaInvalidState(state);
    JS::SetReservedSlot(self, SlotState, JS::Int32Value(int(Configured)));
    args.rval().setUndefined();
    return true;
  }
  if (sub == CProcess) {
    const char* member = kind <= VideoDecode ? "decode" : "encode";
    JS::RootedObject argument(cx, args.get(0).isObject() ? js::CheckedUnwrapStatic(&args.get(0).toObject()) : nullptr);
    const bool valid = (kind == AudioDecode || kind == VideoDecode)
        ? (argument && JS::GetClass(argument) == &kChunkClass)
        : (kind == AudioEncode) ? (argument && JS::GetClass(argument) == &kAudioDataClass)
                                : (argument && JS::GetClass(argument) == &kVideoFrameClass);
    if (!valid)
      return WebKit::Fail(cx, (std::string(kNames[kind]) + "." + member + ": Argument 1 does not implement interface " + kArgInterfaces[kind] + ".").c_str());
    if (current != int(Configured)) {
      JS::RootedValue error(cx);
      if (!CodecError(state, "InvalidStateError",
                      std::string(kNames[kind]) + "." + member + ": " + kEngines[kind] + " must be configured first", &error))
        return false;
      JS_SetPendingException(cx, error);
      return false;
    }
    args.rval().setUndefined();
    return true;
  }
  if (sub == CReset) {
    if (current == int(Closed)) return MediaInvalidState(state);
    JS::SetReservedSlot(self, SlotState, JS::Int32Value(int(Unconfigured)));
    args.rval().setUndefined();
    return true;
  }
  if (current == int(Closed)) return MediaInvalidState(state);
  JS::SetReservedSlot(self, SlotState, JS::Int32Value(int(Closed)));
  args.rval().setUndefined();
  return true;
}
bool CodecStateGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const CodecKind kind = CodecKind(WebKit::Slot(args) / 16);
  JS::RootedObject self(cx, CodecReceiver(cx, args.thisv(), kind));
  if (!self) return WebKit::FailBrand(cx, args, kNames[kind]);
  static const char* const states[3] = {"unconfigured", "configured", "closed"};
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, states[JS::GetReservedSlot(self, SlotState).toInt32()]));
  if (!text) return false;
  args.rval().setString(text);
  return true;
}
bool CodecQueueGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const CodecKind kind = CodecKind(WebKit::Slot(args) / 16);
  JS::RootedObject self(cx, CodecReceiver(cx, args.thisv(), kind));
  if (!self) return WebKit::FailBrand(cx, args, kNames[kind]);
  args.rval().setInt32(0);
  return true;
}
bool CodecOndequeueGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const CodecKind kind = CodecKind(WebKit::Slot(args) / 16);
  JS::RootedObject self(cx, CodecReceiver(cx, args.thisv(), kind));
  if (!self) return WebKit::FailBrand(cx, args, kNames[kind]);
  args.rval().set(JS::GetReservedSlot(self, SlotOndequeue));
  return JS_WrapValue(cx, args.rval());
}
bool CodecOndequeueSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  const CodecKind kind = CodecKind(WebKit::Slot(args) / 16);
  JS::RootedObject self(cx, CodecReceiver(cx, args.thisv(), kind));
  if (!self) return WebKit::FailBrand(cx, args, kNames[kind]);
  if (!MediaHandlerSet(state, self, SlotOndequeue, "dequeue", args.get(0))) return false;
  args.rval().setUndefined();
  return true;
}
}
