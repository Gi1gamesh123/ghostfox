// The 2D drawing state: one array per context plus a save/restore stack of copies.
// Every attribute and the transform live in that one array, so restore() is exact.
#include "web_canvas/internal.h"

#include <cmath>
#include <cstdint>
#include <string>

#include "web_canvas/raster_color.h"

namespace ruyijs::web_canvas {
namespace {
enum FieldKind : uint8_t { kAnyString = 0, kKeyword = 1, kPositive = 2,
                           kNonNegative = 3, kAlpha = 4, kNumber = 5, kBoolean = 6 };

struct FieldSpec {
  const char* name;
  int32_t field;
  uint8_t kind;
  const char* const* allowed;
  int32_t allowedCount;
};

const char* const kLineCapValues[] = {"butt", "round", "square"};
const char* const kLineJoinValues[] = {"round", "bevel", "miter"};
const char* const kTextAlignValues[] = {"start", "end", "left", "right", "center"};
const char* const kTextBaselineValues[] = {"alphabetic", "top", "hanging", "middle",
                                     "ideographic", "bottom"};
const char* const kDirectionValues[] = {"ltr", "rtl", "inherit"};
const char* const kComposite[] = {
    "source-over", "source-in", "source-out", "source-atop", "destination-over",
    "destination-in", "destination-out", "destination-atop", "lighter", "copy", "xor",
    "multiply", "screen", "overlay", "darken", "lighten", "color-dodge", "color-burn",
    "hard-light", "soft-light", "difference", "exclusion", "hue", "saturation",
    "color", "luminosity"};

const FieldSpec kFields[] = {
    {"fontKerning", kFontKerning, kAnyString, nullptr, 0},
    {"fontStretch", kFontStretch, kAnyString, nullptr, 0},
    {"fontVariantCaps", kFontVariantCaps, kAnyString, nullptr, 0},
    {"letterSpacing", kLetterSpacing, kAnyString, nullptr, 0},
    {"wordSpacing", kWordSpacing, kAnyString, nullptr, 0},
    {"textRendering", kTextRendering, kAnyString, nullptr, 0},
    {"lang", kLang, kAnyString, nullptr, 0},
    {"fillStyle", kFillStyle, kAnyString, nullptr, 0},
    {"strokeStyle", kStrokeStyle, kAnyString, nullptr, 0},
    {"lineWidth", kLineWidth, kPositive, nullptr, 0},
    {"lineCap", kLineCap, kKeyword, kLineCapValues, 3},
    {"lineJoin", kLineJoin, kKeyword, kLineJoinValues, 3},
    {"miterLimit", kMiterLimit, kPositive, nullptr, 0},
    {"lineDashOffset", kLineDashOffset, kNumber, nullptr, 0},
    {"font", kFont, kAnyString, nullptr, 0},
    {"textAlign", kTextAlign, kKeyword, kTextAlignValues, 5},
    {"textBaseline", kTextBaseline, kKeyword, kTextBaselineValues, 6},
    {"direction", kDirection, kKeyword, kDirectionValues, 3},
    {"globalAlpha", kGlobalAlpha, kAlpha, nullptr, 0},
    {"globalCompositeOperation", kGlobalCompositeOperation, kKeyword, kComposite, 26},
    {"imageSmoothingEnabled", kImageSmoothingEnabled, kBoolean, nullptr, 0},
    {"shadowBlur", kShadowBlur, kNonNegative, nullptr, 0},
    {"shadowColor", kShadowColor, kAnyString, nullptr, 0},
    {"shadowOffsetX", kShadowOffsetX, kNumber, nullptr, 0},
    {"shadowOffsetY", kShadowOffsetY, kNumber, nullptr, 0},
    {"filter", kFilter, kAnyString, nullptr, 0},
};

// A colour attribute keeps the colour, not the text: the value is re-serialized
// the way CanvasRenderingContext2D::SetStyleFromString stores the nscolor, and a
// string that is not a CSS colour leaves it alone.
bool NormalizeStyle(JSContext* cx, JS::MutableHandleValue value, bool* accepted) {
  if (!value.isString()) return true;  // A gradient or a pattern stays as it is.
  JS::RootedString text(cx, value.toString());
  std::string ascii;
  if (!detail::StringToAscii(cx, text, &ascii)) return false;
  uint8_t rgba[4];
  if (ascii.empty() || !raster::ParseCSSColor(ascii, rgba)) { *accepted = false; return true; }
  const std::string style = raster::CanvasColorStyle(rgba);
  JS::RootedString canonical(cx, JS_NewStringCopyZ(cx, style.c_str()));
  if (!canonical) return false;
  value.setString(canonical);
  return true;
}

// True when the value is one this attribute accepts; an unaccepted value is
// ignored (the attribute keeps its current value) exactly like the specification.
bool Accepts(JSContext* cx, const FieldSpec& spec, JS::HandleValue value, bool* accepted) {
  *accepted = false;
  if (spec.kind == kAnyString) {
    if (value.isString()) *accepted = true;
    if ((spec.field == kFillStyle || spec.field == kStrokeStyle) && value.isObject()) {
      const JSClass* clasp = JS::GetClass(&value.toObject());
      *accepted = clasp == &kGradientClass || clasp == &kPatternClass;
    }
    return true;
  }
  if (spec.kind == kBoolean) {
    *accepted = true;
    return true;
  }
  if (spec.kind == kKeyword) {
    if (!value.isString()) return true;
    JS::RootedString text(cx, value.toString());
    for (int32_t index = 0; index < spec.allowedCount; ++index) {
      bool match = false;
      if (!detail::StringEqualsAscii(cx, text, spec.allowed[index], &match)) return false;
      if (match) { *accepted = true; break; }
    }
    return true;
  }
  double number = 0.0;
  if (!JS::ToNumber(cx, value, &number)) return false;
  if (!std::isfinite(number)) return true;
  switch (spec.kind) {
    case kPositive: *accepted = number > 0.0; break;
    case kNonNegative: *accepted = number >= 0.0; break;
    case kAlpha: *accepted = number >= 0.0 && number <= 1.0; break;
    default: *accepted = true; break;
  }
  return true;
}
}  // namespace

bool Context2DStateGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t index = detail::ContextSlot(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  JS::RootedObject drawing(cx, detail::DrawingState(cx, self));
  if (!drawing) return false;
  return JS_GetElement(cx, drawing, uint32_t(kFields[index].field), args.rval());
}

bool Context2DStateSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t index = detail::ContextSlot(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  args.rval().setUndefined();
  const FieldSpec& spec = kFields[index];
  JS::RootedValue value(cx, args.get(0));
  if (spec.field == kLang) {
    JS::RootedString text(cx, JS::ToString(cx, value));
    if (!text) return false;
    value.setString(text);
  }
  bool accepted = false;
  if (!Accepts(cx, spec, value, &accepted)) return false;
  if (!accepted) return true;
  if (spec.field == kFillStyle || spec.field == kStrokeStyle) {
    if (!NormalizeStyle(cx, &value, &accepted)) return false;
    if (!accepted) return true;
  }
  if (spec.kind == kBoolean) value.setBoolean(JS::ToBoolean(value));
  JS::RootedObject drawing(cx, detail::DrawingState(cx, self));
  if (!drawing) return false;
  return JS_SetElement(cx, drawing, uint32_t(spec.field), value);
}

bool Context2DSave(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  args.rval().setUndefined();
  JS::RootedObject drawing(cx, detail::DrawingState(cx, self));
  JS::RootedObject copy(cx, JS::NewArrayObject(cx, kStateFields));
  if (!drawing || !copy) return false;
  for (int32_t index = 0; index < kStateFields; ++index) {
    JS::RootedValue element(cx);
    if (!JS_GetElement(cx, drawing, uint32_t(index), &element) ||
        !JS_SetElement(cx, copy, uint32_t(index), element)) return false;
  }
  JS::RootedObject stack(cx, &JS::GetReservedSlot(self, kContextStackSlot).toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, stack, &length)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*copy));
  return JS_SetElement(cx, stack, length, value);
}

bool Context2DRestore(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  args.rval().setUndefined();
  JS::RootedObject stack(cx, &JS::GetReservedSlot(self, kContextStackSlot).toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, stack, &length) || length == 0) return true;
  JS::RootedValue last(cx);
  if (!JS_GetElement(cx, stack, length - 1, &last) || !last.isObject()) return false;
  JS::SetReservedSlot(self, kContextStateSlot, last);
  return JS::SetArrayLength(cx, stack, length - 1);
}

bool InstallContext2DState(State& state, WebFace& face) {
  const int32_t count = int32_t(sizeof(kFields) / sizeof(kFields[0]));
  for (int32_t index = 0; index < count; ++index)
    if (!detail::Accessor2D(state, face, kFields[index].name,
                            Context2DStateGet, Context2DStateSet, index)) return false;
  return true;
}
}  // namespace ruyijs::web_canvas
