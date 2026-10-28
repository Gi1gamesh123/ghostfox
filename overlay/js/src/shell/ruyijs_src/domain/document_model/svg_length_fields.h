// The scalar SVGAnimatedLength attributes. List-valued text positions are excluded.
#pragma once
#include "domain/geometry/svg_length_value.h"
#include <iterator>
#include <string_view>
namespace ruyijs::domain::document_model {
struct SvgLengthField {
  const char16_t* tag;
  const char* interfaceName;
  const char* member;
  const char16_t* attribute;
  unsigned axis;  // 0 horizontal, 1 vertical, 2 normalized diagonal.
  geometry::SvgLengthValue initial;
};
#define SVG_LENGTH_FIELD(tag, iface, member, axis, value, unit) \
  {u##tag, "SVG" iface "Element", member, u##member, axis, {value, unit, false}}
#define SVG_LENGTH_RECT(tag, iface, x, y, width, height, unit) \
  SVG_LENGTH_FIELD(tag, iface, "x", 0, x, unit), \
  SVG_LENGTH_FIELD(tag, iface, "y", 1, y, unit), \
  SVG_LENGTH_FIELD(tag, iface, "width", 0, width, unit), \
  SVG_LENGTH_FIELD(tag, iface, "height", 1, height, unit)
inline constexpr SvgLengthField kSvgLengthFields[] = {
  SVG_LENGTH_FIELD("svg", "SVG", "x", 0, 0, 1),
  SVG_LENGTH_FIELD("svg", "SVG", "y", 1, 0, 1),
  SVG_LENGTH_FIELD("svg", "SVG", "width", 0, 100, 2),
  SVG_LENGTH_FIELD("svg", "SVG", "height", 1, 100, 2),
  SVG_LENGTH_RECT("rect", "Rect", 0, 0, 0, 0, 1),
  SVG_LENGTH_FIELD("rect", "Rect", "rx", 0, 0, 1),
  SVG_LENGTH_FIELD("rect", "Rect", "ry", 1, 0, 1),
  SVG_LENGTH_FIELD("circle", "Circle", "cx", 0, 0, 1),
  SVG_LENGTH_FIELD("circle", "Circle", "cy", 1, 0, 1),
  SVG_LENGTH_FIELD("circle", "Circle", "r", 2, 0, 1),
  SVG_LENGTH_FIELD("ellipse", "Ellipse", "cx", 0, 0, 1),
  SVG_LENGTH_FIELD("ellipse", "Ellipse", "cy", 1, 0, 1),
  SVG_LENGTH_FIELD("ellipse", "Ellipse", "rx", 0, 0, 1),
  SVG_LENGTH_FIELD("ellipse", "Ellipse", "ry", 1, 0, 1),
  SVG_LENGTH_FIELD("line", "Line", "x1", 0, 0, 1),
  SVG_LENGTH_FIELD("line", "Line", "y1", 1, 0, 1),
  SVG_LENGTH_FIELD("line", "Line", "x2", 0, 0, 1),
  SVG_LENGTH_FIELD("line", "Line", "y2", 1, 0, 1),
  SVG_LENGTH_RECT("image", "Image", 0, 0, 0, 0, 1),
  SVG_LENGTH_RECT("use", "Use", 0, 0, 0, 0, 1),
  SVG_LENGTH_RECT("foreignObject", "ForeignObject", 0, 0, 0, 0, 1),
  SVG_LENGTH_RECT("pattern", "Pattern", 0, 0, 0, 0, 1),
  SVG_LENGTH_RECT("mask", "Mask", -10, -10, 120, 120, 2),
  SVG_LENGTH_RECT("filter", "Filter", -10, -10, 120, 120, 2),
  SVG_LENGTH_FIELD("marker", "Marker", "refX", 0, 0, 1),
  SVG_LENGTH_FIELD("marker", "Marker", "refY", 1, 0, 1),
  SVG_LENGTH_FIELD("marker", "Marker", "markerWidth", 0, 3, 1),
  SVG_LENGTH_FIELD("marker", "Marker", "markerHeight", 1, 3, 1),
  SVG_LENGTH_FIELD("linearGradient", "LinearGradient", "x1", 0, 0, 2),
  SVG_LENGTH_FIELD("linearGradient", "LinearGradient", "y1", 1, 0, 2),
  SVG_LENGTH_FIELD("linearGradient", "LinearGradient", "x2", 0, 100, 2),
  SVG_LENGTH_FIELD("linearGradient", "LinearGradient", "y2", 1, 0, 2),
  SVG_LENGTH_FIELD("radialGradient", "RadialGradient", "cx", 0, 50, 2),
  SVG_LENGTH_FIELD("radialGradient", "RadialGradient", "cy", 1, 50, 2),
  SVG_LENGTH_FIELD("radialGradient", "RadialGradient", "r", 2, 50, 2),
  SVG_LENGTH_FIELD("radialGradient", "RadialGradient", "fx", 0, 50, 2),
  SVG_LENGTH_FIELD("radialGradient", "RadialGradient", "fy", 1, 50, 2),
  SVG_LENGTH_FIELD("radialGradient", "RadialGradient", "fr", 2, 0, 2),
  SVG_LENGTH_FIELD("", "TextContent", "textLength", 2, 0, 1),
  SVG_LENGTH_FIELD("textPath", "TextPath", "startOffset", 0, 0, 1),
  SVG_LENGTH_RECT("feBlend", "FEBlend", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feColorMatrix", "FEColorMatrix", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feComponentTransfer", "FEComponentTransfer", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feComposite", "FEComposite", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feConvolveMatrix", "FEConvolveMatrix", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feDiffuseLighting", "FEDiffuseLighting", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feDisplacementMap", "FEDisplacementMap", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feDropShadow", "FEDropShadow", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feFlood", "FEFlood", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feGaussianBlur", "FEGaussianBlur", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feImage", "FEImage", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feMerge", "FEMerge", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feMorphology", "FEMorphology", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feOffset", "FEOffset", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feSpecularLighting", "FESpecularLighting", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feTile", "FETile", 0, 0, 100, 100, 2),
  SVG_LENGTH_RECT("feTurbulence", "FETurbulence", 0, 0, 100, 100, 2)
};
#undef SVG_LENGTH_RECT
#undef SVG_LENGTH_FIELD
inline bool SvgLengthFieldApplies(std::u16string_view ns, std::u16string_view local, unsigned field) {
  if (ns != u"http://www.w3.org/2000/svg" || field >= std::size(kSvgLengthFields)) return false;
  const auto& info = kSvgLengthFields[field];
  return *info.tag ? local == info.tag : (local == u"text" || local == u"tspan" || local == u"textPath");
}
inline bool SvgLengthNonNegative(unsigned field) {
  const std::string_view name(kSvgLengthFields[field].member);
  return name == "width" || name == "height" || name == "r" || name == "rx" || name == "ry" ||
      name == "markerWidth" || name == "markerHeight" || name == "textLength";
}
}
