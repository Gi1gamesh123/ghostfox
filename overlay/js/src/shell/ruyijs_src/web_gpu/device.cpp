#include "web_gpu/internal.h"
#include "web_gpu/value_objects.h"

#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_gpu {
namespace {

bool CreateBuffer(State& state, JS::HandleObject self, const JS::CallArgs& args,
                  JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  JS::RootedValue descriptor(cx, args.get(0));
  if (!descriptor.isObject())
    return ThrowError(state, "TypeError", nullptr,
                      "createBuffer requires a descriptor dictionary");
  JS::RootedObject dictionary(cx, &descriptor.toObject());
  JS::RootedValue sizeValue(cx), usageValue(cx);
  if (!JS_GetProperty(cx, dictionary, "size", &sizeValue) ||
      !JS_GetProperty(cx, dictionary, "usage", &usageValue))
    return false;
  double size = 0.0;
  if (!sizeValue.isNumber() || !ReadDouble(cx, sizeValue, &size))
    return ThrowError(state, "TypeError", nullptr, "buffer size must be a number");
  uint32_t usage = 0;
  if (!usageValue.isNumber() || !ReadUint(cx, usageValue, &usage))
    return ThrowError(state, "TypeError", nullptr, "buffer usage must be a number");
  const double maximum = Limit(state, "maxBufferSize");
  if (size <= 0.0 || size > maximum || size != static_cast<double>(static_cast<uint32_t>(size)) ||
      static_cast<uint32_t>(size) % 4 != 0 || usage == 0)
    return ThrowOperation(state, "invalid buffer descriptor");
  JS::RootedString label(cx);
  if (!ReadLabel(cx, dictionary, &label)) return false;
  if (JS::GetReservedSlot(self, kDeviceDestroyed).toBoolean())
    return ThrowOperation(state, "the device is destroyed");
  JS::RootedObject buffer(cx, NewBuffer(state, self, size, usage, label));
  if (!buffer) return false;
  result.setObject(*buffer);
  return true;
}

bool CreateTexture(State& state, JS::HandleObject self, const JS::CallArgs& args,
                   JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  JS::RootedValue descriptor(cx, args.get(0));
  if (!descriptor.isObject())
    return ThrowError(state, "TypeError", nullptr,
                      "createTexture requires a descriptor dictionary");
  JS::RootedObject dictionary(cx, &descriptor.toObject());
  JS::RootedValue sizeValue(cx), formatValue(cx), usageValue(cx);
  if (!JS_GetProperty(cx, dictionary, "size", &sizeValue) ||
      !JS_GetProperty(cx, dictionary, "format", &formatValue) ||
      !JS_GetProperty(cx, dictionary, "usage", &usageValue))
    return false;
  if (!sizeValue.isObject())
    return ThrowError(state, "TypeError", nullptr, "texture size must be a dictionary");
  if (!formatValue.isString())
    return ThrowError(state, "TypeError", nullptr, "texture format must be a string");
  if (!usageValue.isNumber())
    return ThrowError(state, "TypeError", nullptr, "texture usage must be a number");
  uint32_t usage = 0;
  if (!ReadUint(cx, usageValue, &usage) || usage == 0)
    return ThrowOperation(state, "invalid texture usage");
  JS::RootedObject size(cx, &sizeValue.toObject());
  JS::RootedValue widthValue(cx), heightValue(cx), depthValue(cx), mipValue(cx),
      sampleValue(cx), dimensionValue(cx);
  if (!JS_GetProperty(cx, size, "width", &widthValue) ||
      !JS_GetProperty(cx, size, "height", &heightValue) ||
      !JS_GetProperty(cx, size, "depthOrArrayLayers", &depthValue) ||
      !JS_GetProperty(cx, dictionary, "mipLevelCount", &mipValue) ||
      !JS_GetProperty(cx, dictionary, "sampleCount", &sampleValue) ||
      !JS_GetProperty(cx, dictionary, "dimension", &dimensionValue))
    return false;
  double width = 0.0, height = 1.0, depth = 1.0, mips = 1.0, samples = 1.0;
  if (!widthValue.isNumber() || !ReadDouble(cx, widthValue, &width))
    return ThrowError(state, "TypeError", nullptr, "texture width must be a number");
  if (!heightValue.isUndefined() && !ReadDouble(cx, heightValue, &height))
    return ThrowError(state, "TypeError", nullptr, "texture height must be a number");
  if (!depthValue.isUndefined() && !ReadDouble(cx, depthValue, &depth))
    return ThrowError(state, "TypeError", nullptr, "texture depth must be a number");
  if (!mipValue.isUndefined() && !ReadDouble(cx, mipValue, &mips))
    return ThrowError(state, "TypeError", nullptr, "texture mipLevelCount must be a number");
  if (!sampleValue.isUndefined() && !ReadDouble(cx, sampleValue, &samples))
    return ThrowError(state, "TypeError", nullptr, "texture sampleCount must be a number");
  JS::RootedString dimension(cx, JS_NewStringCopyZ(cx, "2d"));
  if (!dimension) return false;
  if (!dimensionValue.isUndefined()) {
    JS::RootedString text(cx, JS::ToString(cx, dimensionValue));
    if (!text) return false;
    dimension.set(text);
  }
  std::string dimensionText;
  if (!ToUTF8(cx, dimension, &dimensionText)) return false;
  const char* limit = dimensionText == "1d" ? "maxTextureDimension1D"
                    : dimensionText == "3d" ? "maxTextureDimension3D"
                                            : "maxTextureDimension2D";
  if (dimensionText != "1d" && dimensionText != "2d" && dimensionText != "3d")
    return ThrowOperation(state, "invalid texture dimension");
  if (width < 1.0 || height < 1.0 || depth < 1.0 || mips < 1.0 ||
      width > Limit(state, limit) || height > Limit(state, limit) ||
      depth > Limit(state, limit) || (samples != 1.0 && samples != 4.0))
    return ThrowOperation(state, "invalid texture descriptor");
  JS::RootedString label(cx);
  if (!ReadLabel(cx, dictionary, &label)) return false;
  if (JS::GetReservedSlot(self, kDeviceDestroyed).toBoolean())
    return ThrowOperation(state, "the device is destroyed");
  JS::RootedString format(cx, formatValue.toString());
  JS::RootedObject texture(cx,
      NewTexture(state, self, size, format, usage,
                 static_cast<uint32_t>(mips), static_cast<uint32_t>(samples),
                 dimension, label));
  if (!texture) return false;
  result.setObject(*texture);
  return true;
}

bool CreateEncoder(State& state, JS::HandleObject self, const JS::CallArgs& args,
                   JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  JS::RootedValue descriptor(cx, args.get(0));
  if (!descriptor.isUndefined() && !descriptor.isObject())
    return ThrowError(state, "TypeError", nullptr,
                      "createCommandEncoder requires a descriptor dictionary");
  JS::RootedString label(cx, JS_NewStringCopyZ(cx, ""));
  if (!label) return false;
  if (descriptor.isObject()) {
    JS::RootedObject dictionary(cx, &descriptor.toObject());
    if (!ReadLabel(cx, dictionary, &label)) return false;
  }
  if (JS::GetReservedSlot(self, kDeviceDestroyed).toBoolean())
    return ThrowOperation(state, "the device is destroyed");
  JS::RootedObject encoder(cx, NewCommandEncoder(state, label));
  if (!encoder) return false;
  result.setObject(*encoder);
  return true;
}

}  // namespace

JSObject* NewDevice(State& state, JS::HandleObject proto) {
  JSContext* cx = state.cx;
  const JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject device(cx, state.kit.New(&kDeviceClass, proto));
  if (!device) return nullptr;
  JS::SetReservedSlot(device, kDeviceFeatures, JS::NullValue());
  JS::SetReservedSlot(device, kDeviceLimits, JS::NullValue());
  JS::SetReservedSlot(device, kDeviceQueue, JS::NullValue());
  JS::SetReservedSlot(device, kDeviceLost, JS::NullValue());
  JS::SetReservedSlot(device, kDeviceDestroyed, JS::BooleanValue(false));
  for (int32_t index = 0; index < kDeviceHandlerCount; ++index)
    JS::SetReservedSlot(device, kDeviceHandlerBase + index, JS::NullValue());
  if (!state.events->Attach(device)) return nullptr;
  return device.get();
}

bool DeviceProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  State* state = ValueReceiver(cx, args.thisv(), &kDeviceClass, &self);
  if (!state) return false;
  auto read = [&]() -> bool {
  switch (WebKit::Slot(args)) {
    case kDeviceFeatures: {
      JS::RootedValue cached(cx, JS::GetReservedSlot(self, kDeviceFeatures));
      if (cached.isObject()) {
        args.rval().set(cached);
        return true;
      }
      JS::RootedObject features(cx, NewFeatures(*state, 1));
      if (!features) return false;
      JS::SetReservedSlot(self, kDeviceFeatures, JS::ObjectValue(*features));
      args.rval().setObject(*features);
      return true;
    }
    case kDeviceLimits: {
      JS::RootedValue cached(cx, JS::GetReservedSlot(self, kDeviceLimits));
      if (cached.isObject()) {
        args.rval().set(cached);
        return true;
      }
      JS::RootedObject limits(cx);
      if (!BuildLimits(*state, &limits)) return false;
      JS::SetReservedSlot(self, kDeviceLimits, JS::ObjectValue(*limits));
      args.rval().setObject(*limits);
      return true;
    }
    case kDeviceQueue: {
      JS::RootedValue cached(cx, JS::GetReservedSlot(self, kDeviceQueue));
      if (cached.isObject()) {
        args.rval().set(cached);
        return true;
      }
      JS::RootedObject queue(cx, state->kit.New(&kQueueClass, state->queue.prototype));
      if (!queue) return false;
      JS::SetReservedSlot(self, kDeviceQueue, JS::ObjectValue(*queue));
      args.rval().setObject(*queue);
      return true;
    }
    default: {
      JS::RootedValue cached(cx, JS::GetReservedSlot(self, kDeviceLost));
      if (cached.isObject()) {
        args.rval().set(cached);
        return true;
      }
      JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
      if (!promise) return false;
      JS::SetReservedSlot(self, kDeviceLost, JS::ObjectValue(*promise));
      if (JS::GetReservedSlot(self, kDeviceDestroyed).toBoolean()) {
        JS::RootedValue reason(cx);
        if (!LostReason(*state, &reason) ||
            !JS::ResolvePromise(cx, promise, reason))
          return false;
      }
      args.rval().setObject(*promise);
      return true;
    }
  }
  };
  { JSAutoRealm realm(cx, self); if (!read()) return false; }
  return JS_WrapValue(cx, args.rval());
}

bool DeviceMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!CheckDevice(cx, args.thisv(), &self)) return false;
  switch (WebKit::Slot(args)) {
    case kDeviceCreateBuffer:
      return CreateBuffer(*state, self, args, args.rval());
    case kDeviceCreateTexture:
      return CreateTexture(*state, self, args, args.rval());
    case kDeviceCreateCommandEncoder:
      return CreateEncoder(*state, self, args, args.rval());
    default: {
      JS::SetReservedSlot(self, kDeviceDestroyed, JS::BooleanValue(true));
      JS::RootedValue lost(cx, JS::GetReservedSlot(self, kDeviceLost));
      if (lost.isObject()) {
        JS::RootedObject promise(cx, &lost.toObject());
        JS::RootedValue reason(cx);
        if (!LostReason(*state, &reason) ||
            !JS::ResolvePromise(cx, promise, reason))
          return false;
      }
      args.rval().setUndefined();
      return true;
    }
  }
}

}  // namespace ruyijs::web_gpu
