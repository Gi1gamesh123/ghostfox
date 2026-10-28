// TextMetrics from the font owner. The 2D context stores the ctx.font shorthand;
// measurement is the shaping owner's (web_geometry), which selects the same fonts
// the element layout uses, so a width here and an offsetWidth there agree.
#include "web_canvas/internal.h"

#include <memory>

#include "js/Realm.h"
#include "js/String.h"
#include "mozilla/Range.h"
#include "web_geometry/system_font.h"

namespace ruyijs::web_canvas {
const JSClass kMetricsClass = {"TextMetrics", JSCLASS_HAS_RESERVED_SLOTS(kMetricCount)};
namespace {
const char* const kNames[kMetricCount] = {
    "width", "actualBoundingBoxLeft", "actualBoundingBoxRight",
    "actualBoundingBoxAscent", "actualBoundingBoxDescent", "fontBoundingBoxAscent",
    "fontBoundingBoxDescent", "emHeightAscent", "emHeightDescent", "hangingBaseline",
    "alphabeticBaseline", "ideographicBaseline"};
bool MetricGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kMetricsClass, &self, "Illegal invocation")) return false;
  args.rval().set(JS::GetReservedSlot(self, slot));
  return true;
}
bool StateString(JSContext* cx, JS::HandleObject drawing, int32_t field, std::string* out) {
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, drawing, uint32_t(field), &value)) return false;
  if (!value.isString()) return true;
  JS::RootedString text(cx, value.toString());
  JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, text);
  if (!bytes) return false;
  *out = bytes.get();
  return true;
}
}  // namespace

web_geometry::SystemFonts& detail::Fonts(State& state) {
  if (!state.fonts) state.fonts = std::make_unique<web_geometry::SystemFonts>();
  return *state.fonts;
}

bool detail::StringUnits(JSContext* cx, JS::HandleString text, std::u16string* out) {
  const size_t length = JS_GetStringLength(text);
  out->assign(length, 0);
  if (length == 0) return true;
  return JS_CopyStringChars(cx, mozilla::Range<char16_t>(out->data(), length), text);
}

bool MeasureText(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  if (!argc) return WebKit::Fail(cx, "measureText requires text");
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  if (!text) return false;
  std::u16string units;
  if (!detail::StringUnits(cx, text, &units)) return false;
  // The font shorthand lives in the drawing state, so save/restore applies to it.
  JS::RootedObject drawing(cx, detail::DrawingState(cx, self));
  JS::RootedValue shorthand(cx);
  if (!drawing || !JS_GetElement(cx, drawing, uint32_t(kFont), &shorthand)) return false;
  detail::CanvasFont font;
  if (shorthand.isString()) {
    JS::RootedString value(cx, shorthand.toString());
    JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, value);
    if (!bytes) return false;
    detail::ParseCanvasFont(std::string(bytes.get()), &font);
  }
  if (!font.valid && !detail::ParseCanvasFont("10px sans-serif", &font))
    return WebKit::Fail(cx, "canvas font default is not measurable");
  web_geometry::FontInput input;
  input.family = font.family;
  input.size = font.size;
  input.weight = font.weight;
  input.italic = font.italic;
  web_geometry::CanvasTextMetrics metrics;
  if (!detail::Fonts(s).MeasureCanvas(units, input, &metrics))
    return WebKit::Fail(cx, "canvas text measurement failed");
  std::string baseline, align, direction;
  if (!StateString(cx, drawing, kTextBaseline, &baseline) ||
      !StateString(cx, drawing, kTextAlign, &align) ||
      !StateString(cx, drawing, kDirection, &direction)) return false;
  double anchor = metrics.alphabetic_baseline;
  if (baseline == "top") anchor = metrics.em_ascent;
  else if (baseline == "hanging") anchor = metrics.hanging_baseline;
  else if (baseline == "middle") anchor = (metrics.em_ascent - metrics.em_descent) * .5;
  else if (baseline == "ideographic") anchor = metrics.ideographic_baseline;
  else if (baseline == "bottom") anchor = -metrics.em_descent;
  const bool rtl = direction == "rtl";
  const bool right = align == "right" || (align == "start" && rtl) || (align == "end" && !rtl);
  const float offset = align == "center" ? .5 * metrics.width : right ? metrics.width : 0;
  JS::RootedObject result(cx, s.kit.New(&kMetricsClass, s.metrics.prototype));
  if (!result) return false;
  // Firefox reports the advance as a float, the ink box unrounded relative to the
  // origin on the alphabetic baseline, and the font box rounded to whole pixels.
  const double values[kMetricCount] = {
      double(metrics.width), offset - metrics.ink_left, metrics.ink_right - offset,
      -metrics.ink_top - anchor, metrics.ink_bottom + anchor,
      metrics.font_ascent - anchor, metrics.font_descent + anchor,
      metrics.em_ascent - anchor, metrics.em_descent + anchor,
      metrics.hanging_baseline - anchor, metrics.alphabetic_baseline - anchor,
      metrics.ideographic_baseline - anchor};
  for (int slot = 0; slot < kMetricCount; ++slot)
    JS::SetReservedSlot(result, uint32_t(slot), JS::NumberValue(values[slot]));
  args.rval().setObject(*result);
  return true;
}

bool InstallTextMetrics(State& s) {
  JS::RootedObject parent(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define("TextMetrics", &kMetricsClass, parent, WebKit::Reject, &s.metrics)) return false;
  for (int slot = 0; slot < kMetricCount; ++slot)
    if (!s.kit.Accessor(s.metrics, "TextMetrics", kNames[slot], MetricGet, nullptr, slot)) return false;
  return true;
}
}  // namespace ruyijs::web_canvas
