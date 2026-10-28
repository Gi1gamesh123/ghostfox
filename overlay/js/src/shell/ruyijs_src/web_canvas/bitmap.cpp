// A bitmap snapshot and a rendering context both use the Canvas owner's bytes.
#include "web_canvas/internal.h"
#include "js/Realm.h"
#include <cstring>

namespace ruyijs::web_canvas {
const JSClass kBitmapClass = {"ImageBitmap", JSCLASS_HAS_RESERVED_SLOTS(3)};
const JSClass kBitmapRendererClass = {"ImageBitmapRenderingContext", JSCLASS_HAS_RESERVED_SLOTS(1)};
namespace {
bool BitmapGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kBitmapClass, &self, "ImageBitmap")) return false;
  args.rval().set(JS::GetReservedSlot(self, slot));
  return true;
}
void CloseBitmap(JS::HandleObject self) {
  JS::SetReservedSlot(self, 0, JS::Int32Value(0));
  JS::SetReservedSlot(self, 1, JS::Int32Value(0));
  JS::SetReservedSlot(self, 2, JS::NullValue());
}
bool BitmapClose(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kBitmapClass, &self, "ImageBitmap")) return false;
  CloseBitmap(self);
  args.rval().setUndefined();
  return true;
}
bool RendererCanvas(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kBitmapRendererClass, &self, "ImageBitmapRenderingContext")) return false;
  args.rval().set(JS::GetReservedSlot(self, 0));
  return true;
}
bool TransferFrom(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  const bool legacy=WebKit::Slot(args)==1;
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kBitmapRendererClass, &self, "ImageBitmapRenderingContext")) return false;
  if (!argc || (legacy && args.get(0).isNullOrUndefined()))
    return WebKit::Fail(cx,"ImageBitmap argument required");
  JS::RootedObject canvas(cx, &JS::GetReservedSlot(self, 0).toObject());
  CanvasBinding* binding = nullptr;
  if (!EnsureBitmap(s, canvas, &binding)) return false;
  args.rval().setUndefined();
  if (args.get(0).isNullOrUndefined())
    return detail::ResizeBitmap(s, *binding, binding->width, binding->height);
  JS::RootedObject image(cx);
  if (!WebKit::Check(cx, args[0], &kBitmapClass, &image, "Expected ImageBitmap")) return false;
  JS::RootedValue source(cx, JS::GetReservedSlot(image, 2));
  if (!source.isObject()) return ThrowDOMException(s, "InvalidStateError", "ImageBitmap is closed");
  uint32_t w = uint32_t(JS::GetReservedSlot(image, 0).toNumber());
  uint32_t h = uint32_t(JS::GetReservedSlot(image, 1).toNumber());
  JS::RootedObject bytes(cx, &source.toObject());
  if (uint64_t(w) * h > JS_GetTypedArrayByteLength(bytes) / 4)
    return ThrowDOMException(s, "InvalidStateError", "Bitmap pixels exceed the Canvas allocation limit");
  if (!detail::ResizeBitmap(s, *binding, binding->width, binding->height)) return false;
  if (binding->bitmap) {
    JS::AutoCheckCannotGC nogc;
    uint8_t* from = detail::ViewBytes(bytes);
    uint8_t* to = detail::ViewBytes(binding->bitmap);
    for (uint32_t y = 0; y < std::min(h, binding->height); ++y)
      std::memcpy(to + size_t(y)*binding->width*4, from + size_t(y)*w*4,
                  size_t(std::min(w, binding->width))*4);
  }
  CloseBitmap(image);
  return true;
}
}
bool NewCanvasBitmap(State& s, JS::HandleObject canvas, JS::MutableHandleObject out) {
  CanvasBinding* binding = nullptr;
  if (!EnsureBitmap(s, canvas, &binding)) return false;
  JSContext* cx = s.cx;
  JS::RootedObject object(cx, s.kit.New(&kBitmapClass, s.bitmap.prototype));
  JS::RootedObject pixels(cx, JS_NewUint8ClampedArray(cx,
      binding->bitmap ? JS_GetTypedArrayLength(binding->bitmap) : 0));
  if (!object || !pixels) return false;
  if (binding->bitmap) {
    JS::AutoCheckCannotGC nogc;
    std::memcpy(detail::ViewBytes(pixels), detail::ViewBytes(binding->bitmap),
                JS_GetTypedArrayByteLength(pixels));
  }
  JS::SetReservedSlot(object, 0, JS::NumberValue(binding->width));
  JS::SetReservedSlot(object, 1, JS::NumberValue(binding->height));
  JS::SetReservedSlot(object, 2, JS::ObjectValue(*pixels));
  out.set(object);
  return true;
}
bool BitmapContext(State& s, JS::HandleObject canvas, JS::MutableHandleValue out) {
  CanvasBinding* b = detail::EnsureCanvas(s, canvas);
  if (!b) return false;
  if (!b->bitmapContext) {
    JS::RootedObject object(s.cx, s.kit.New(&kBitmapRendererClass, s.bitmapRenderer.prototype));
    if (!object) return false;
    JS::SetReservedSlot(object, 0, JS::ObjectValue(*canvas));
    b->bitmapContext = object;
  }
  out.setObject(*b->bitmapContext);
  return true;
}
bool InstallBitmap(State& s) {
  JS::RootedObject parent(s.cx, JS::GetRealmObjectPrototype(s.cx));
  return s.kit.Define("ImageBitmap", &kBitmapClass, parent, WebKit::Reject, &s.bitmap) &&
    s.kit.Accessor(s.bitmap, "ImageBitmap", "width", BitmapGet, nullptr, 0) &&
    s.kit.Accessor(s.bitmap, "ImageBitmap", "height", BitmapGet, nullptr, 1) &&
    s.kit.Method(s.bitmap, "ImageBitmap", "close", BitmapClose, 0, 0) &&
    s.kit.Define("ImageBitmapRenderingContext", &kBitmapRendererClass, parent, WebKit::Reject, &s.bitmapRenderer) &&
    s.kit.Accessor(s.bitmapRenderer, "ImageBitmapRenderingContext", "canvas", RendererCanvas, nullptr, 0) &&
    s.kit.Method(s.bitmapRenderer, "ImageBitmapRenderingContext", "transferFromImageBitmap", TransferFrom, 1, 0) &&
    s.kit.Method(s.bitmapRenderer,"ImageBitmapRenderingContext","transferImageBitmap",TransferFrom,1,1);
}
}
