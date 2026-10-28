#include "web_geometry/svg_length_context.h"
#include "domain/styling/css_numeric_value.h"
#include <cstdlib>
namespace ruyijs::web_geometry {
namespace {
float DetachedFactor(unsigned unit) {
  if (unit == 2) return .01f;
  if (unit == 4 || unit == 18) return 16;
  if (unit == 12 || unit == 19) return .5f;
  if (unit == 16 || unit == 17) return 16.f * 1.2f;
  if (unit >= 22) return 0;
  return 1;
}
bool ReadNode(State& s, JS::HandleObject object, document_model::GeometryNode* node,
                JS::MutableHandleObject parent) {
  JS::RootedValue value(s.cx, JS::ObjectValue(*object));
  JS::RootedObjectVector children(s.cx);
  return s.svgPorts.node && s.svgPorts.node(s.svgPorts.context, value, node, parent, &children);
}
}
bool SvgLengthFactor(State& s, JS::HandleObject object, unsigned unit, float* out) {
  if (domain::geometry::SvgLengthAbsolute(unit)) {
    *out = domain::geometry::SvgLengthAbsoluteScale(5, unit); return true;
  }
  if (!JS::GetReservedSlot(object, LengthMode).toInt32()) { *out = DetachedFactor(unit); return true; }
  JS::RootedValue node(s.cx, JS::GetReservedSlot(object, LengthNode));
  if (!JS_WrapValue(s.cx, &node)) return false;
  JS::RootedObject element(s.cx, &node.toObject());
  if (unit == 2) {
    const unsigned field = JS::GetReservedSlot(object, LengthField).toInt32();
    if (!SvgLengthViewport(s, element, domain::document_model::kSvgLengthFields[field].axis, out)) return false;
    *out /= 100.f; return true;
  }
  if (unit >= 22) {
    double width = 0, height = 0;
    if (!s.svgPorts.lengthViewport || !s.svgPorts.lengthViewport(s.svgPorts.context, node, &width, &height)) return false;
    *out = float(unit == 22 ? width : unit == 23 ? height : unit == 24 ? std::min(width,height) : std::max(width,height)) / 100.f;
    return true;
  }
  return SvgLengthFontFactor(s, element, unit, out);
}
bool Realm::SvgLengthViewportSize(double* width, double* height) {
  State& s = *state_;
  double x, y;
  return s.boxPorts.viewport && s.boxPorts.viewport(s.boxPorts.context, width, height, &x, &y);
}
bool SvgLengthFontFactor(State& s, JS::HandleObject element, unsigned unit, float* out) {
  JS::RootedObject target(s.cx, element), parent(s.cx);
  document_model::GeometryNode source;
  if (!ReadNode(s, target, &source, &parent)) return false;
  const bool root = unit == 13 || unit == 17 || (unit >= 18 && unit <= 21);
  if (root) {
    JS::RootedValue value(s.cx, JS::ObjectValue(*element));
    if (!s.svgPorts.lengthRoot || !s.svgPorts.lengthRoot(s.svgPorts.context, value, &target)) return false;
    if (!target) { *out = DetachedFactor(unit); return true; }
    if (!ReadNode(s, target, &source, &parent)) return false;
  }
  if (!source.connected) { *out = DetachedFactor(unit); return true; }
  web_styling::ComputedStyle style;
  if (!s.svgPorts.style || !s.svgPorts.style(s.svgPorts.context, target, &style)) return false;
  if (unit == 3 || unit == 13) { *out = float(style.fontSize); return true; }
  FontInput input;
  input.family = style.Get("font-family"); input.size = style.fontSize;
  input.italic = style.Get("font-style") == "italic" || style.Get("font-style") == "oblique";
  const auto weight = style.Get("font-weight");
  if (!weight.empty()) input.weight = unsigned(std::strtoul(weight.c_str(), nullptr, 10));
  if (unit == 16 || unit == 17) {
    const auto height = style.Get("line-height");
    const auto value = domain::styling::CssNumericResolve(height, style.fontSize, style.rootFontSize, style.fontSize);
    if (height != "normal" && value.kind) *out = float(value.number);
    else {
      double content, normal;
      if (!s.fonts.LineMetrics(input, &content, &normal)) return false;
      *out = float(normal);
    }
    return true;
  }
  float ex, ch, ic, cap;
  if (!s.fonts.SvgLengthMetrics(input, &ex, &ch, &ic, &cap)) return false;
  *out = unit == 4 || unit == 18 ? ex : unit == 12 || unit == 19 ? ch : unit == 14 || unit == 20 ? ic : cap;
  return true;
}
}
