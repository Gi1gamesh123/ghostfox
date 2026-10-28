#include <algorithm>
#include "web_audio/state.h"

#include <cmath>

#include "js/Array.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/Object.h"
#include "js/experimental/TypedData.h"

namespace ruyijs::web_audio {
namespace {

const char* kReceiver = "Web Audio member called on an incompatible receiver";

State* Self(const JS::CallArgs& args) {
  return static_cast<State*>(WebKit::Owner(args));
}

// One AudioBuffer dimension, from either createBuffer's positional arguments or
// AudioBuffer's dictionary; out-of-range values raise a RangeError.
bool Dimension(JSContext* cx, JS::HandleValue value, double low, double high,
               const char* message, double* out) {
  double number = 0.0;
  if (!JS::ToNumber(cx, value, &number)) return false;
  if (!std::isfinite(number) || number < low || number > high)
    return WebKit::FailKind(cx, message, JSEXN_RANGEERR);
  *out = number;
  return true;
}

// Builds an AudioBuffer instance: one cached Float32Array per channel, all
// zeroed. getChannelData always returns the same array, so the silent render is
// observable and stable.
JSObject* BuildBuffer(State& state, JS::HandleObject proto, int32_t channels,
                      int32_t length, double rate) {
  JSContext* cx = state.cx;
  if (channels < 1) channels = 1;
  if (channels > 32) channels = 32;
  if (length < 0) length = 0;
  if (!(rate > 0)) rate = state.model.sample_rate();
  JS::RootedObject object(cx, state.kit.New(&kBufferClass, proto));
  if (!object) return nullptr;
  JS::RootedObject data(cx, JS::NewArrayObject(cx, channels));
  if (!data) return nullptr;
  for (int32_t index = 0; index < channels; ++index) {
    JS::RootedObject channel(cx, JS_NewFloat32Array(cx, size_t(length)));
    if (!channel) return nullptr;
    JS::RootedValue value(cx, JS::ObjectValue(*channel));
    if (!JS_DefineElement(cx, data, uint32_t(index), value, JSPROP_ENUMERATE))
      return nullptr;
  }
  JS::SetReservedSlot(object, kBufferDataSlot, JS::ObjectValue(*data));
  JS::SetReservedSlot(object, kBufferLengthSlot, JS::Int32Value(length));
  JS::SetReservedSlot(object, kBufferCountSlot, JS::Int32Value(channels));
  JS::SetReservedSlot(object, kBufferRateSlot, JS::DoubleValue(rate));
  return object.get();
}

}  // namespace

JSObject* NewBuffer(State& state, int32_t channels, int32_t length, double rate) {
  return BuildBuffer(state, state.buffer.prototype, channels, length, rate);
}
bool CopyBufferContents(State& state, JS::HandleObject from, JS::HandleObject to) {
  JSContext* cx = state.cx;
  JS::RootedObject source(cx, &JS::GetReservedSlot(from, kBufferDataSlot).toObject());
  JS::RootedObject target(cx, &JS::GetReservedSlot(to, kBufferDataSlot).toObject());
  const int32_t channels = std::min(JS::GetReservedSlot(from, kBufferCountSlot).toInt32(),
                                    JS::GetReservedSlot(to, kBufferCountSlot).toInt32());
  JS::RootedValue a(cx), b(cx);
  for (int32_t index = 0; index < channels; ++index) {
    if (!JS_GetElement(cx, source, uint32_t(index), &a) || !JS_GetElement(cx, target, uint32_t(index), &b))
      return false;
    JSObject* src = &a.toObject();
    JSObject* dst = &b.toObject();
    size_t srcLength = 0, dstLength = 0;
    bool shared = false;
    JS::AutoCheckCannotGC nogc;
    float* s = JS_GetFloat32ArrayData(src, &shared, nogc);
    float* d = JS_GetFloat32ArrayData(dst, &shared, nogc);
    srcLength = JS_GetTypedArrayLength(src);
    dstLength = JS_GetTypedArrayLength(dst);
    if (s && d) std::copy(s, s + std::min(srcLength, dstLength), d);
  }
  return true;
}

// createBuffer(numberOfChannels, length, sampleRate). Missing arguments keep the
// donor's defaults; present ones are range checked against the shared bounds.
bool CreateBuffer(State& state, const JS::CallArgs& args,
                  JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  int32_t channels = 1, length = 0;
  double rate = state.model.sample_rate();
  double number = 0.0;
  if (args.length() > 0) {
    if (!Dimension(cx, args[0], 1, 32, "AudioBuffer numberOfChannels is out of range",
                   &number)) return false;
    channels = int32_t(number);
  }
  if (args.length() > 1) {
    if (!Dimension(cx, args[1], 0, 2147483647.0, "AudioBuffer length is out of range",
                   &number)) return false;
    length = int32_t(number);
  }
  if (args.length() > 2) {
    if (!Dimension(cx, args[2], 1, 96000, "AudioBuffer sampleRate is out of range",
                   &number)) return false;
    rate = number;
  }
  JS::RootedObject buffer(cx, BuildBuffer(state, state.buffer.prototype, channels,
                                          length, rate));
  if (!buffer) return false;
  result.setObject(*buffer);
  return true;
}

// The donor's AudioBuffer template: called as a function it yields undefined;
// with new it builds an instance, reading the dictionary dimensions when given.
bool BufferCtor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.isConstructing()) {
    args.rval().setUndefined();
    return true;
  }
  State* state = Self(args);
  JS::RootedObject proto(cx);
  if (!state->kit.ConstructProto(args, state->buffer.prototype, &proto)) return false;
  int32_t channels = 1, length = 0;
  double rate = state->model.sample_rate();
  if (args.length() && args[0].isObject()) {
    JS::RootedObject options(cx, &args[0].toObject());
    JS::RootedValue value(cx);
    double number = 0.0;
    if (!JS_GetProperty(cx, options, "length", &value)) return false;
    if (!value.isUndefined()) {
      if (!Dimension(cx, value, 0, 2147483647.0, "AudioBuffer length is out of range",
                     &number)) return false;
      length = int32_t(number);
    }
    if (!JS_GetProperty(cx, options, "numberOfChannels", &value)) return false;
    if (!value.isUndefined()) {
      if (!Dimension(cx, value, 1, 32, "AudioBuffer numberOfChannels is out of range",
                     &number)) return false;
      channels = int32_t(number);
    }
    if (!JS_GetProperty(cx, options, "sampleRate", &value)) return false;
    if (!value.isUndefined()) {
      if (!Dimension(cx, value, 1, 96000, "AudioBuffer sampleRate is out of range",
                     &number)) return false;
      rate = number;
    }
  }
  JS::RootedObject object(cx, BuildBuffer(*state, proto, channels, length, rate));
  if (!object) return false;
  args.rval().setObject(*object);
  return true;
}

bool BufferGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kBufferClass, &self, kReceiver)) return false;
  switch (WebKit::Slot(args)) {
    case 0:
      args.rval().set(JS::GetReservedSlot(self, kBufferLengthSlot));
      return true;
    case 1:
      args.rval().set(JS::GetReservedSlot(self, kBufferCountSlot));
      return true;
    case 2:
      args.rval().set(JS::GetReservedSlot(self, kBufferRateSlot));
      return true;
    default: {
      const double rate = JS::GetReservedSlot(self, kBufferRateSlot).toNumber();
      const int32_t length = JS::GetReservedSlot(self, kBufferLengthSlot).toInt32();
      args.rval().setNumber(rate > 0 ? double(length) / rate : 0.0);
      return true;
    }
  }
}

}  // namespace ruyijs::web_audio
