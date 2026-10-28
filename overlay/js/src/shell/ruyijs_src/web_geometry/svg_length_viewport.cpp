#include "web_geometry/svg_length_context.h"
#include "domain/styling/css_numeric_value.h"
#include <algorithm>
#include <cmath>
namespace ruyijs::web_geometry {
namespace {
bool Read(State& s, JS::HandleObject object, SvgInput* input, JS::MutableHandleObject parent) {
  JS::RootedValue value(s.cx, JS::ObjectValue(*object));
  JS::RootedObjectVector children(s.cx);
  return s.svgPorts.node && s.svgPorts.node(s.svgPorts.context, value, &input->node, parent, &children) &&
      s.svgPorts.style && s.svgPorts.style(s.svgPorts.context, object, &input->style);
}
float Dimension(const SvgInput& input, const char* name, float reference, float fallback) {
  auto css = input.style.Get(name);
  if (css.empty() || css == "auto") css = SvgAttribute(input, name);
  const auto value = domain::styling::CssNumericResolve(css, input.style.fontSize, input.style.rootFontSize, reference);
  return value.kind && std::isfinite(value.number) ? float(std::max(0., value.number)) : fallback;
}
}
bool SvgLengthViewport(State& s, JS::HandleObject element, unsigned axis, float* out) {
  SvgInput input;
  JS::RootedObject cursor(s.cx, element), parent(s.cx);
  JS::RootedObjectVector ancestors(s.cx);
  if (!Read(s, cursor, &input, &parent)) return false;
  const bool ownSvg = input.node.localName == u"svg";
  unsigned depth = 0;
  while (parent) {
    if (++depth >= 256) return WebKit::Fail(s.cx, "SVG length viewport budget exceeded");
    cursor = parent;
    SvgInput current;
    if (!Read(s, cursor, &current, &parent)) return false;
    if (current.node.localName == u"foreignObject") break;
    if (current.node.localName == u"svg" && !ancestors.append(cursor)) return false;
  }
  if (ancestors.empty() && ownSvg && !ancestors.append(element)) return false;
  if (ancestors.empty()) { *out = 1; return true; }
  float width = 300, height = 150;
  for (size_t index = ancestors.length(); index-- > 0;) {
    cursor = ancestors[index];
    SvgInput current;
    if (!Read(s, cursor, &current, &parent)) return false;
    bool viewBox = false;
    {
      document_model::SvgViewBoxValue live;
      JS::RootedValue value(s.cx, JS::ObjectValue(*cursor));
      bool supported = false; void* factory = nullptr;
      if (!s.svgPorts.viewBoxRead || !s.svgPorts.viewBoxRead(s.svgPorts.context, value, &supported, &live, &factory)) return false;
      if (supported && live.present) { width = live.fields[2]; height = live.fields[3]; viewBox = true; }
    }
    if (viewBox) continue;
    if (!current.node.connected && index + 1 == ancestors.length()) { width = height = 0; continue; }
    if (index + 1 == ancestors.length() && parent) {
      // Resolve outer percentages against the original containing block.
      auto* factory = static_cast<Realm*>(current.node.factory);
      std::array<double,4> rectangle{}; bool present = false;
      if (factory && !factory->MeasureElement(parent, &rectangle, &present, true)) return false;
      if (present) { width = float(rectangle[2]); height = float(rectangle[3]); }
    }
    const bool outer = index + 1 == ancestors.length();
    width = Dimension(current, "width", width, outer ? 300 : width);
    height = Dimension(current, "height", height, outer ? 150 : height);
  }
  *out = axis == 0 ? width : axis == 1 ? height : float(std::hypot(double(width), double(height)) / std::sqrt(2.));
  if (*out == 0) *out = 1e-20f;
  return true;
}
}
