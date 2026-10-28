#include "web_geometry/svg.h"
#include "web_geometry/system_font.h"
#include <cstdlib>

namespace ruyijs::web_geometry {
namespace {
bool Read(State& s, JS::HandleObject node, SvgInput* input, JS::MutableHandleObject parent,
           JS::MutableHandleObjectVector children) {
  JS::RootedValue value(s.cx, JS::ObjectValue(*node));
  if (!s.svgPorts.node(s.svgPorts.context, value, &input->node, parent, children)) return false;
  if (input->node.element && !s.svgPorts.style(s.svgPorts.context, node, &input->style)) return false;
  auto transform = input->style.Get("transform");
  if (transform.empty() || transform == "none") transform = SvgAttribute(*input, "transform");
  input->transform = SvgTransform(transform);
  return true;
}
bool Text(State& s, const SvgInput& input, float scale, svg::SvgTextMeasurement* out) {
  FontInput font;
  font.family = input.style.Get("font-family");
  font.size = input.style.fontSize;
  font.italic = input.style.Get("font-style") != "normal";
  const auto weight = input.style.Get("font-weight");
  if (!weight.empty()) font.weight = unsigned(std::strtoul(weight.c_str(), nullptr, 10));
  font.language = SvgAttribute(input, "lang");
  if (font.language.empty()) font.language = SvgAttribute(input, "xml:lang");
  font.scale = scale;
  font.transform = input.transform;
  font.x = SvgLength(input, "x", input.viewportWidth) + SvgLength(input, "dx", input.viewportWidth);
  font.y = SvgLength(input, "y", input.viewportHeight) + SvgLength(input, "dy", input.viewportHeight);
  if (!s.fonts.Measure(input.node.text, font, out))
    return WebKit::Fail(s.cx, "SVG system-font measurement exceeds supported input or font resources");
  const auto anchor = input.style.Get("text-anchor");
  const float shift = anchor == "middle" ? out->length/2 : anchor == "end" ? out->length : 0;
  out->box[0] -= shift;
  for (auto& box : out->characters) box[0] -= shift;
  return true;
}
bool Visit(State& s, JS::HandleObject element, SvgInput input, const SvgOptions& options,
           svg::SvgMatrix matrix, unsigned depth, unsigned* visited,
           svg::SvgTextMeasurement* out, bool* present) {
  if (depth >= 128 || ++*visited > 4096) return WebKit::Fail(s.cx, "SVG geometry tree budget exceeded");
  JS::RootedObject parent(s.cx);
  JS::RootedObjectVector children(s.cx);
  if (!Read(s, element, &input, &parent, &children)) return false;
  *present = false;
  if (!input.node.connected || input.style.Get("display") == "none") return true;
  matrix = matrix.Then(input.transform);
  if (input.node.localName == u"svg") {
    input.viewportWidth = SvgLength(input, "width", input.viewportWidth, input.viewportWidth);
    input.viewportHeight = SvgLength(input, "height", input.viewportHeight, input.viewportHeight);
  }
  if (input.node.textContent) {
    if (!Text(s, input, matrix.Scale(), out)) return false;
    *present = !out->characters.empty();
    return true;
  }
  if (input.node.localName != u"g" && input.node.localName != u"svg" && input.node.localName != u"a" &&
      input.node.localName != u"switch" && input.node.localName != u"symbol" && input.node.localName != u"defs") {
    if (!input.node.graphics) return true;
    if (!SvgShape(input, options, &out->box, present))
      return WebKit::Fail(s.cx, "SVG shape geometry is not yet supported for this input");
    return true;
  }
  svg::SvgBounds bounds;
  for (size_t i = 0; i < children.length(); ++i) {
    JS::RootedObject child(s.cx, children[i]);
    SvgInput childInput = input;
    JS::RootedObject childParent(s.cx);
    JS::RootedObjectVector grandchildren(s.cx);
    if (!Read(s, child, &childInput, &childParent, &grandchildren)) return false;
    if (!childInput.node.graphics) continue;
    const auto transform = childInput.transform;
    const bool isText = childInput.node.textContent;
    svg::SvgTextMeasurement measured;
    bool included = false;
    if (!Visit(s, child, std::move(childInput), options, matrix, depth+1, visited, &measured, &included)) return false;
    if (included) bounds.Include(isText ? measured.transformed : transform.Apply(measured.box));
  }
  *present = bounds.present; out->box = bounds.box; return true;
}
}
bool SvgMeasure(State& s, JS::HandleObject element, const SvgOptions& options,
                 svg::SvgTextMeasurement* out, bool* rendered) {
  *out = {}; *rendered = false;
  SvgInput input;
  JS::RootedObject parent(s.cx);
  JS::RootedObjectVector children(s.cx);
  if (!Read(s, element, &input, &parent, &children)) return false;
  if (!input.node.connected || input.style.Get("display") == "none") return true;
  svg::SvgMatrix matrix;
  unsigned depth = 0;
  bool viewport = false;
  JS::RootedObject ancestor(s.cx, parent);
  while (ancestor) {
    if (++depth > 128) return WebKit::Fail(s.cx, "SVG geometry ancestor budget exceeded");
    SvgInput outer;
    if (!Read(s, ancestor, &outer, &parent, &children)) return false;
    if (outer.style.Get("display") == "none") return true;
    matrix = outer.transform.Then(matrix);
    if (!viewport && outer.node.localName == u"svg") {
      input.viewportWidth = SvgLength(outer, "width", 300, 300);
      input.viewportHeight = SvgLength(outer, "height", 150, 150);
      viewport = true;
    }
    ancestor = parent;
  }
  *rendered = true;
  unsigned visited = 0;
  bool present = false;
  return Visit(s, element, std::move(input), options, matrix, 0, &visited, out, &present);
}
}
