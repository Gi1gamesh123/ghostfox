#include "web_geometry/svg.h"
#include "domain/styling/css_numeric_value.h"
#include "domain/geometry/svg_transform.h"

namespace ruyijs::web_geometry {
std::string SvgAttribute(const SvgInput& input, const char* name) {
  std::u16string key;
  for (; *name; ++name) key += char16_t(*name);
  for (const auto& attr : input.node.attributes)
    if (attr.name == key) return std::string(attr.value.begin(), attr.value.end());
  return {};
}
double SvgLength(const SvgInput& input, const char* name, double percent, double fallback) {
  auto value = SvgAttribute(input, name);
  if (value.empty()) return fallback;
  const auto parsed = ruyijs::domain::styling::CssNumericResolve(value, input.style.fontSize, 16, percent);
  return parsed.kind && std::isfinite(parsed.number) ? parsed.number : fallback;
}
svg::SvgMatrix SvgTransform(const std::string& value) {
  return svg::ParseSvgTransform(value);
}
}
