// ImageData: a real Uint8ClampedArray of width*height*4 pixels plus the two
// dimensions. The type is owned by this realm and created only by its factory.
#include "web_canvas/internal.h"

#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"

namespace ruyijs::web_canvas {
const JSClass kImageDataClass = {"ImageData", JSCLASS_HAS_RESERVED_SLOTS(kImageSlotCount)};

namespace {
JSObject* MakeImageData(JSContext* cx, JS::HandleObject proto, int32_t width, int32_t height,
                        JS::MutableHandleObject out) {
  if (width < 0 || height < 0) return nullptr;
  const uint64_t area = uint64_t(width) * uint64_t(height);
  if (area > kCanvasAreaLimit) return nullptr;
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kImageDataClass, proto));
  JS::RootedObject pixels(cx, JS_NewUint8ClampedArray(cx, size_t(area) * 4u));
  if (!object || !pixels) return nullptr;
  JS::SetReservedSlot(object, kImageWidthSlot, JS::Int32Value(width));
  JS::SetReservedSlot(object, kImageHeightSlot, JS::Int32Value(height));
  JS::SetReservedSlot(object, kImageDataSlot, JS::ObjectValue(*pixels));
  out.set(object);
  return object;
}
}  // namespace

JSObject* NewImageData(State& state, int32_t width, int32_t height,
                       JS::MutableHandleObject out) {
  return MakeImageData(state.cx, state.imageData.prototype, width, height, out);
}

bool ImageDataCtor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  NativeConstructorTrace trace(cx, args);
  JS::RootedObject prototype(cx);
  if (!state.kit.ConstructProto(args, state.imageData.prototype, &prototype)) return false;
  if (args.length() == 0) return WebKit::Fail(cx, "ImageData requires arguments");
  if (args.get(0).isObject() &&
      JS_GetArrayBufferViewType(&args.get(0).toObject()) == JS::Scalar::Uint8Clamped) {
    // new ImageData(data, width) / new ImageData(data, width, height).
    JS::RootedObject data(cx, &args.get(0).toObject());
    const size_t length = JS_GetTypedArrayByteLength(data);
    int32_t width = 0, height = 0;
    if (!detail::ToLong(cx, args.get(1), &width)) return false;
    if (width <= 0)
      return ThrowDOMException(state, "IndexSizeError", "The source width is not a positive number.");
    if (args.length() > 2 && !args.get(2).isUndefined()) {
      if (!detail::ToLong(cx, args.get(2), &height)) return false;
    } else {
      const size_t stride = size_t(width) * 4u;
      if (length % stride != 0)
        return ThrowDOMException(state, "IndexSizeError", "The data length is not a multiple of 4 * width.");
      height = int32_t(length / stride);
    }
    if (height <= 0 || uint64_t(width) * uint64_t(height) * 4ull != length)
      return ThrowDOMException(state, "IndexSizeError", "The data length does not match the dimensions.");
    JS::RootedObject image(cx);
    if (!MakeImageData(cx, prototype, width, height, &image)) {
      if (JS_IsExceptionPending(cx)) return false;
      return ThrowDOMException(state, "IndexSizeError", "The image is too large.");
    }
    JS::SetReservedSlot(image, kImageDataSlot, JS::ObjectValue(*data));
    if (!state.kit.helpers->Object("ImageData", image)) return false;
    args.rval().setObject(*image);
    return trace.Finish(true);
  }
  // new ImageData(width, height).
  if (args.length() < 2) return WebKit::Fail(cx, "ImageData requires width and height");
  int32_t width = 0, height = 0;
  if (!detail::ToLong(cx, args.get(0), &width) || !detail::ToLong(cx, args.get(1), &height))
    return false;
  if (width <= 0 || height <= 0)
    return ThrowDOMException(state, "IndexSizeError", "The source width or height is zero.");
  JS::RootedObject image(cx);
  if (!MakeImageData(cx, prototype, width, height, &image)) {
    if (JS_IsExceptionPending(cx)) return false;
    return ThrowDOMException(state, "IndexSizeError", "The image is too large.");
  }
  if (!state.kit.helpers->Object("ImageData", image)) return false;
  args.rval().setObject(*image);
  return trace.Finish(true);
}

bool ImageDataGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kImageDataClass, &self, "ImageData")) return false;
  args.rval().set(JS::GetReservedSlot(self, slot));
  return true;
}

bool CreateImageData(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  if (args.length() == 0) return WebKit::Fail(cx, "createImageData requires arguments");
  int32_t width = 0, height = 0;
  if (args.get(0).isObject() && JS::GetClass(&args.get(0).toObject()) == &kImageDataClass) {
    JS::RootedObject source(cx, &args.get(0).toObject());
    width = JS::GetReservedSlot(source, kImageWidthSlot).toInt32();
    height = JS::GetReservedSlot(source, kImageHeightSlot).toInt32();
  } else {
    if (args.length() < 2) return WebKit::Fail(cx, "createImageData requires width and height");
    if (!detail::ToLong(cx, args.get(0), &width) || !detail::ToLong(cx, args.get(1), &height))
      return false;
    if (width < 0 || height < 0)
      return WebKit::Fail(cx, "createImageData dimensions must be non-negative");
    if (width == 0 || height == 0)
      return ThrowDOMException(state, "IndexSizeError", "The source width or height is zero.");
  }
  if (uint64_t(width) * uint64_t(height) > kCanvasAreaLimit)
    return ThrowDOMException(state, "IndexSizeError", "The image is too large.");
  JS::RootedObject image(cx);
  if (!NewImageData(state, width, height, &image)) {
    if (JS_IsExceptionPending(cx)) return false;
    return ThrowDOMException(state, "IndexSizeError", "The image is too large.");
  }
  if (!state.kit.helpers->Object("ImageData", image)) return false;
  args.rval().setObject(*image);
  return true;
}

bool InstallImageData(State& state) {
  WebKit& kit = state.kit;
  JSContext* cx = state.cx;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  if (!kit.Define("ImageData", &kImageDataClass, objectProto, ImageDataCtor, &state.imageData))
    return false;
  return kit.Accessor(state.imageData, "ImageData", "width", ImageDataGet, nullptr,
                      kImageWidthSlot) &&
         kit.Accessor(state.imageData, "ImageData", "height", ImageDataGet, nullptr,
                      kImageHeightSlot) &&
         kit.Accessor(state.imageData, "ImageData", "data", ImageDataGet, nullptr,
                      kImageDataSlot);
}
}  // namespace ruyijs::web_canvas
