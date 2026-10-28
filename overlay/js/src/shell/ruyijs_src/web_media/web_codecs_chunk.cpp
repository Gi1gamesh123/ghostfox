// EncodedAudioChunk / EncodedVideoChunk: pure data objects carrying a copied
// BufferSource. They share one class; the reserved kind slot distinguishes the
// two interface names for errors and the type enumeration name.
#include "web_media/web_codecs.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include <cstring>
#include <vector>
namespace ruyijs::web_media {
const JSClass kChunkClass = {"WebCodecs chunk", JSCLASS_HAS_RESERVED_SLOTS(5)};
namespace {
enum ChunkSlot { ChunkKind = 0, ChunkType = 1, ChunkTimestamp = 2, ChunkDuration = 3, ChunkData = 4 };
const char* Name(bool video) { return video ? "EncodedVideoChunk" : "EncodedAudioChunk"; }
const char* TypeName(bool video) { return video ? "EncodedVideoChunkType" : "EncodedAudioChunkType"; }

JSObject* ChunkReceiver(JSContext* cx, JS::HandleValue self, bool video) {
  JSObject* object = self.isObject() ? js::CheckedUnwrapStatic(&self.toObject()) : nullptr;
  if (object && JS::GetClass(object) == &kChunkClass &&
      JS::GetReservedSlot(object, ChunkKind).toBoolean() == video) return object;
  return nullptr;
}
bool CopyBufferSource(JSContext* cx, JS::HandleValue value, std::vector<uint8_t>* bytes) {
  if (!value.isObject()) return false;
  JSObject* source = js::CheckedUnwrapStatic(&value.toObject());
  if (!source) return false;
  const bool view = JS_IsArrayBufferViewObject(source);
  if (!view && !JS::IsArrayBufferObject(source)) return false;
  size_t length = view ? JS_GetArrayBufferViewByteLength(source) : JS::GetArrayBufferByteLength(source);
  bool shared = false;
  JS::AutoCheckCannotGC noGC(cx);
  const uint8_t* data = view
      ? static_cast<const uint8_t*>(JS_GetArrayBufferViewData(source, &shared, noGC))
      : JS::GetArrayBufferData(source, &shared, noGC);
  bytes->assign(data, data + length);
  return true;
}
}
bool ChunkConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  const bool video = WebKit::Slot(args) != 0;
  WebFace& face = video ? state.encodedVideoChunk : state.encodedAudioChunk;
  if (!args.isConstructing())
    return trace.Finish(WebKit::Fail(cx, (std::string(Name(video)) + " constructor: 'new' is required").c_str()));
  JS::RootedObject prototype(cx);
  if (!state.kit.ConstructProto(args, face.prototype, &prototype)) return false;
  if (!argc)
    return trace.Finish(WebKit::Fail(cx, (std::string(Name(video)) + " constructor: At least 1 argument required, but only 0 passed").c_str()));
  JS::RootedObject init(cx, args.get(0).isObject() ? &args.get(0).toObject() : nullptr);
  JS::RootedValue data(cx), timestamp(cx), type(cx), duration(cx);
  if (init) {
    if (!JS_GetProperty(cx, init, "data", &data) || !JS_GetProperty(cx, init, "timestamp", &timestamp) ||
        !JS_GetProperty(cx, init, "type", &type) || !JS_GetProperty(cx, init, "duration", &duration)) return false;
  }
  // Required members are validated in reverse declaration order: data first.
  if (data.isUndefined())
    return trace.Finish(WebKit::Fail(cx, (std::string(Name(video)) + " constructor: Missing required 'data' member of " + Name(video) + "Init.").c_str()));
  if (timestamp.isUndefined())
    return trace.Finish(WebKit::Fail(cx, (std::string(Name(video)) + " constructor: Missing required 'timestamp' member of " + Name(video) + "Init.").c_str()));
  if (type.isUndefined())
    return trace.Finish(WebKit::Fail(cx, (std::string(Name(video)) + " constructor: Missing required 'type' member of " + Name(video) + "Init.").c_str()));
  std::string typeText;
  JS::RootedString typeString(cx, type.toString());
  if (!ToUTF8(cx, typeString, &typeText)) return false;
  const int typeIndex = typeText == "key" ? 0 : typeText == "delta" ? 1 : -1;
  if (typeIndex < 0) {
    const std::string message = std::string(Name(video)) + " constructor: '" + typeText +
        "' (value of 'type' member of " + Name(video) + "Init) is not a valid value for enumeration " + TypeName(video) + ".";
    return trace.Finish(WebKit::Fail(cx, message.c_str()));
  }
  double timestampValue = 0;
  if (!JS::ToNumber(cx, timestamp, &timestampValue)) return false;
  std::vector<uint8_t> bytes;
  if (!CopyBufferSource(cx, data, &bytes))
    return trace.Finish(WebKit::Fail(cx, (std::string(Name(video)) + " constructor: Argument 1 does not implement interface BufferSource.").c_str()));
  JS::RootedObject buffer(cx, JS::NewArrayBuffer(cx, bytes.size()));
  if (!buffer) return false;
  if (!bytes.empty()) {
    size_t length = 0;
    bool shared = false;
    uint8_t* out = nullptr;
    JS::GetArrayBufferLengthAndData(buffer, &length, &shared, &out);
    std::memcpy(out, bytes.data(), bytes.size());
  }
  double durationValue = 0;
  if (!duration.isUndefined() && !duration.isNull() && !JS::ToNumber(cx, duration, &durationValue)) return false;
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kChunkClass, prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, ChunkKind, JS::BooleanValue(video));
  JS::SetReservedSlot(object, ChunkType, JS::Int32Value(typeIndex));
  JS::SetReservedSlot(object, ChunkTimestamp, JS::DoubleValue(timestampValue));
  JS::SetReservedSlot(object, ChunkDuration, duration.isUndefined() || duration.isNull() ? JS::NullValue() : JS::DoubleValue(durationValue));
  JS::SetReservedSlot(object, ChunkData, JS::ObjectValue(*buffer));
  args.rval().setObject(*object);
  return trace.Finish(true);
}
bool ChunkCopyTo(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const bool video = WebKit::Slot(args) != 0;
  JS::RootedObject self(cx, ChunkReceiver(cx, args.thisv(), video));
  if (!self) return WebKit::FailBrand(cx, args, Name(video));
  if (!argc)
    return WebKit::Fail(cx, (std::string(Name(video)) + ".copyTo: At least 1 argument required, but only 0 passed").c_str());
  JSObject* destination = args.get(0).isObject() ? js::CheckedUnwrapStatic(&args.get(0).toObject()) : nullptr;
  const bool view = destination && JS_IsArrayBufferViewObject(destination);
  if (!destination || (!view && !JS::IsArrayBufferObject(destination)))
    return WebKit::Fail(cx, (std::string(Name(video)) + ".copyTo: Argument 1 does not implement interface BufferSource.").c_str());
  JS::RootedObject source(cx, JS::GetReservedSlot(self, ChunkData).toObjectOrNull());
  const size_t byteLength = JS::GetArrayBufferByteLength(source);
  const size_t destinationLength = view ? JS_GetArrayBufferViewByteLength(destination) : JS::GetArrayBufferByteLength(destination);
  if (destinationLength < byteLength)
    return WebKit::Fail(cx, (std::string(Name(video)) + ".copyTo: Destination ArrayBuffer smaller than source " +
                             Name(video)).c_str());
  const size_t count = byteLength;
  if (count) {
    JS::AutoCheckCannotGC noGC(cx);
    bool shared = false;
    const uint8_t* from = JS::GetArrayBufferData(source, &shared, noGC);
    uint8_t* to = view ? static_cast<uint8_t*>(JS_GetArrayBufferViewData(destination, &shared, noGC))
                       : JS::GetArrayBufferData(destination, &shared, noGC);
    if (from && to) std::memcpy(to, from, count);
  }
  args.rval().setUndefined();
  return true;
}
bool ChunkGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  const bool video = slot >= 10;
  const int field = slot % 10;
  JS::RootedObject self(cx, ChunkReceiver(cx, args.thisv(), video));
  if (!self) return WebKit::FailBrand(cx, args, Name(video));
  if (field == 0) {
    JS::RootedString text(cx, JS_NewStringCopyZ(cx, JS::GetReservedSlot(self, ChunkType).toInt32() == 0 ? "key" : "delta"));
    if (!text) return false;
    args.rval().setString(text);
  } else if (field == 1) {
    args.rval().set(JS::GetReservedSlot(self, ChunkTimestamp));
  } else if (field == 2) {
    args.rval().set(JS::GetReservedSlot(self, ChunkDuration));
  } else {
    JS::RootedObject source(cx, JS::GetReservedSlot(self, ChunkData).toObjectOrNull());
    args.rval().setNumber(double(JS::GetArrayBufferByteLength(source)));
  }
  return JS_WrapValue(cx, args.rval());
}
}
