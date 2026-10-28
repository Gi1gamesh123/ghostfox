// Per-canvas state: the one RGBA byte store and the one 2D context. Both hang off
// the binding list the realm owns, never off a second tree or a JS-side cache.
#include "web_canvas/internal.h"
#include "web_canvas/realm.h"

namespace ruyijs::web_canvas::detail {
namespace {
struct Default {
  int32_t field;
  uint8_t kind;  // 0 string, 1 number, 2 boolean
  const char* text;
  double number;
};
// The drawing state defaults from the HTML specification's 2D context.
const Default kDefaults[] = {
    {kFillStyle, 0, "#000000", 0},
    {kStrokeStyle, 0, "#000000", 0},
    {kLineWidth, 1, nullptr, 1.0},
    {kLineCap, 0, "butt", 0},
    {kLineJoin, 0, "miter", 0},
    {kMiterLimit, 1, nullptr, 10.0},
    {kLineDashOffset, 1, nullptr, 0.0},
    {kFont, 0, "10px sans-serif", 0},
    {kTextAlign, 0, "start", 0},
    {kTextBaseline, 0, "alphabetic", 0},
    {kDirection, 0, "inherit", 0},
    {kGlobalAlpha, 1, nullptr, 1.0},
    {kGlobalCompositeOperation, 0, "source-over", 0},
    {kImageSmoothingEnabled, 2, nullptr, 1},
    {kShadowBlur, 1, nullptr, 0.0},
    {kShadowColor, 0, "transparent", 0},
    {kShadowOffsetX, 1, nullptr, 0.0},
    {kShadowOffsetY, 1, nullptr, 0.0},
    {kFilter, 0, "none", 0},
    {kFontKerning, 0, "auto", 0},
    {kFontStretch, 0, "normal", 0},
    {kFontVariantCaps, 0, "normal", 0},
    {kLetterSpacing, 0, "0px", 0},
    {kWordSpacing, 0, "0px", 0},
    {kTextRendering, 0, "auto", 0},
    {kLang, 0, "inherit", 0},
};
}  // namespace

CanvasBinding* EnsureCanvas(State& state, JS::HandleObject canvas) {
  return state.FindOrCreate(canvas);
}

bool ResizeBitmap(State& state, CanvasBinding& binding, uint32_t width, uint32_t height) {
  JSContext* cx = state.cx;
  const uint64_t area = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
  JS::RootedObject bitmap(cx);
  if (width > 0 && height > 0 && area <= kCanvasAreaLimit) {
    bitmap = JS_NewUint8ClampedArray(cx, static_cast<size_t>(area) * 4u);
    if (!bitmap) return false;
  }
  binding.bitmap = bitmap;
  binding.width = width;
  binding.height = height;
  return true;
}

JSObject* BitmapOf(CanvasBinding* binding) {
  return binding ? binding->bitmap.get() : nullptr;
}

// The canvas store is premultiplied, like Firefox's surface; the PNG encoder
// takes straight RGBA and divides by the alpha the way nsPNGEncoder does.
std::string PixelsOf(JSContext* cx, CanvasBinding* binding) {
  std::string bytes;
  JSObject* bitmap = BitmapOf(binding);
  if (!bitmap) return bytes;
  const size_t length = JS_GetTypedArrayByteLength(bitmap);
  bool shared = false;
  JS::AutoCheckCannotGC nogc;
  const uint8_t* data = static_cast<const uint8_t*>(
      JS_GetArrayBufferViewData(bitmap, &shared, nogc));
  if (!data || length == 0) return bytes;
  bytes.assign(reinterpret_cast<const char*>(data), length);
  for (size_t index = 0; index + 3 < length; index += 4) {
    const unsigned alpha = uint8_t(bytes[index + 3]);
    if (alpha == 255) continue;
    if (alpha == 0) {
      bytes[index] = bytes[index + 1] = bytes[index + 2] = 0;
      continue;
    }
    for (size_t channel = 0; channel < 3; ++channel) {
      const unsigned value = uint8_t(bytes[index + channel]);
      bytes[index + channel] = char((value * 255u + alpha / 2) / alpha);
    }
  }
  // ImageEncoder::ExtractDataInternal: the encoder input is perturbed after
  // UnpremultiplyAndPerturbHOSTARGB's un-premultiply, the formula used above.
  if (bytes.size() == size_t(binding->width) * size_t(binding->height) * 4u)
    PerturbExportedPixels(reinterpret_cast<uint8_t*>(bytes.data()), binding->width,
                          binding->height, int32_t(binding->width * 4u), 0, 0, 1);
  return bytes;
}

JSObject* DrawingState(JSContext* cx, JS::HandleObject context) {
  JS::Value slot = JS::GetReservedSlot(context, kContextStateSlot);
  return slot.isObject() ? &slot.toObject() : nullptr;
}

bool SetDrawingState(JSContext* cx, JS::HandleObject context, JS::HandleObject state) {
  JS::SetReservedSlot(context, kContextStateSlot, JS::ObjectValue(*state));
  return true;
}

bool GetMatrix(JSContext* cx, JS::HandleObject context, double components[6]) {
  JS::RootedObject drawing(cx, DrawingState(cx, context));
  if (!drawing) return false;
  return StateMatrix(cx, drawing, components);
}

bool SetMatrix(JSContext* cx, JS::HandleObject context, const double components[6]) {
  JS::RootedObject drawing(cx, DrawingState(cx, context));
  if (!drawing) return false;
  for (int32_t index = 0; index < 6; ++index) {
    JS::RootedValue value(cx, JS::NumberValue(components[index]));
    if (!JS_SetElement(cx, drawing, uint32_t(kMatrix + index), value)) return false;
  }
  return true;
}

JSObject* ContextCanvas(State& state, JS::HandleObject context) {
  JS::Value slot = JS::GetReservedSlot(context, kContextCanvasSlot);
  return slot.isObject() ? &slot.toObject() : nullptr;
}

CanvasBinding* BindingOf(State& state, JS::HandleObject context) {
  JS::Value slot = JS::GetReservedSlot(context, kContextCanvasSlot);
  if (!slot.isObject()) return nullptr;
  return state.Find(&slot.toObject());
}

bool StateMatrix(JSContext* cx, JS::HandleObject state, double out[6]) {
  for (int32_t index = 0; index < 6; ++index) {
    JS::RootedValue value(cx);
    if (!JS_GetElement(cx, state, uint32_t(kMatrix + index), &value) ||
        !value.isNumber())
      return false;
    out[index] = value.toNumber();
  }
  return true;
}

JSObject* NewDrawingState(JSContext* cx, JS::MutableHandleObject out) {
  JS::RootedObject state(cx, JS::NewArrayObject(cx, kStateFields));
  if (!state) return nullptr;
  for (const Default& entry : kDefaults) {
    JS::RootedValue value(cx);
    if (entry.kind == 0) {
      JS::RootedString text(cx, JS_NewStringCopyZ(cx, entry.text));
      if (!text) return nullptr;
      value.setString(text);
    } else if (entry.kind == 1) {
      value.setNumber(entry.number);
    } else {
      value.setBoolean(entry.number != 0);
    }
    if (!JS_SetElement(cx, state, uint32_t(entry.field), value)) return nullptr;
  }
  JS::RootedObject dash(cx, JS::NewArrayObject(cx, 0));
  JS::RootedValue dashValue(cx);
  if (!dash) return nullptr;
  dashValue.setObject(*dash);
  if (!JS_SetElement(cx, state, kLineDash, dashValue)) return nullptr;
  static const double kIdentity[6] = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
  for (int32_t index = 0; index < 6; ++index) {
    JS::RootedValue value(cx, JS::NumberValue(kIdentity[index]));
    if (!JS_SetElement(cx, state, uint32_t(kMatrix + index), value)) return nullptr;
  }
  out.set(state);
  return state;
}
}  // namespace ruyijs::web_canvas::detail
