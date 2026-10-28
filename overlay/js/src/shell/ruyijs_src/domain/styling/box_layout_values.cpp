#include "domain/styling/box_layout_internal.h"
#include "domain/styling/css_numeric_value.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace ruyijs::domain::styling {
std::string BoxStyle::Get(const char* name) const {
  for (const auto& value : values) if (value.first == name) return value.second;
  return {};
}
double BoxLength(const BoxStyle& style, const char* name, double percent, double fallback) {
  const auto text = style.Get(name);
  if (!std::isfinite(percent) && text.find('%') != text.npos) return fallback;
  const auto parsed = CssNumericResolve(text, style.font_size, style.root_font_size,
                                       std::isfinite(percent) ? percent : 0);
  return parsed.kind && (parsed.kind == CssLength || parsed.number == 0)
      ? BoxRound(parsed.number) : fallback;
}
bool BoxViewportLength(const std::string& text, double width, double height, double* out) {
  const char* units[] = {"vmin", "vmax", "vw", "vh"};
  for (const char* unit : units) {
    const size_t n = std::strlen(unit);
    if (text.size() <= n || text.compare(text.size() - n, n, unit) != 0) continue;
    char* end = nullptr;
    const std::string number = text.substr(0, text.size() - n);
    const double value = std::strtod(number.c_str(), &end);
    if (end != number.c_str() + number.size() || !std::isfinite(value)) return false;
    const double base = unit[1] == 'm' ? (unit[2] == 'i' ? std::min(width, height) : std::max(width, height))
                                       : (unit[1] == 'w' ? width : height);
    *out = BoxRound(value * base / 100);
    return true;
  }
  return false;
}
bool BoxReplaced(const BoxNode& node) {
  return node.tag == "iframe" || node.tag == "img" || node.tag == "canvas" ||
         node.tag == "video" || node.tag == "object" || node.tag == "embed";
}
bool BoxAbsolute(const BoxNode& node) {
  // A text node copies its parent's style but is never itself positioned.
  if (node.text) return false;
  const auto position = node.style.Get("position");
  return position == "absolute" || position == "fixed";
}
void BoxEdges(const BoxNode& node, double width, BoxLayout* box) {
  const char* sides[] = {"top", "right", "bottom", "left"};
  for (unsigned i = 0; i < 4; ++i) {
    const auto suffix = std::string(sides[i]);
    box->margin[i] = BoxLength(node.style, ("margin-" + suffix).c_str(), width, 0);
    if (node.tag == "body" && !(node.style.specified_margin_mask & (1u << i))) box->margin[i] = 8;
    box->padding[i] = std::max(0.0, BoxLength(node.style, ("padding-" + suffix).c_str(), width, 0));
    box->edge[i] = std::max(0.0, BoxLength(node.style, ("border-" + suffix + "-width").c_str(), width, 0));
    if (node.tag == "iframe" && !(node.style.specified_border_mask & (1u << i))) box->edge[i] = node.no_frame_border ? 0 : 2;
    const bool text_input = node.tag == "input" && (node.input_type.empty() || node.input_type == "text" ||
        node.input_type == "password" || node.input_type == "email" || node.input_type == "url" ||
        node.input_type == "tel" || node.input_type == "search");
    if (text_input) {
      if (!(node.style.specified_border_mask & (1u << i))) box->edge[i] = 2;
      if (!(node.style.specified_padding_mask & (1u << i))) box->padding[i] = i % 2 ? 2 : 1;
    }
  }
}
double BoxLimit(const BoxStyle& style, bool width, double available, double value, double extras) {
  const auto minimum = BoxLength(style, width ? "min-width" : "min-height", available, 0);
  const auto maximum = BoxLength(style, width ? "max-width" : "max-height", available,
                                 std::numeric_limits<double>::infinity());
  const auto subtract = style.Get("box-sizing") == "border-box" ? extras : 0;
  return std::max(0.0, std::max(minimum - subtract, std::min(value, maximum - subtract)));
}
}
