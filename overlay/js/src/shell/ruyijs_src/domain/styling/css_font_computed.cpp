/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// Font values are computed once in the existing cascade, before geometry reads them.
#include "domain/styling/css_font_value.h"
#include "domain/styling/css_numeric_value.h"
#include "domain/styling/stylesheet.h"
#include <cmath>
#include <limits>
namespace ruyijs::domain::styling {
namespace {
std::string FontValue(const StyleDeclaration& style, const std::string& name) {
  auto value = style.get_property_value(name);
  return value.empty() ? StyleResolutionService::initial_value(name) : value;
}
double FontPixels(const StyleDeclaration& style) {
  if (style.font_computed) return style.computed_font_size;
  auto number = CssNumericParse(FontValue(style, "font-size"));
  return number.kind == CssLength ? number.number : 16;
}
}
void CssComputeFont(StyleDeclaration& style, const StyleDeclaration& parent, double root_size) {
  for (const auto& row : CssFontDefaults())
    if (!style.all().count(row.name)) style.set_property(row.name, StyleResolutionService::initial_value(row.name));
  const double inherited = FontPixels(parent);
  auto size = CssLower(FontValue(style, "font-size"));
  auto number = CssNumericResolve(size, inherited, root_size, inherited);
  if (size == "smaller") number = {CssLength, float(inherited) * (1.f / 1.2f)};
  if (size == "larger") number = {CssLength, float(inherited) * 1.2f};
  const char* keywords[] = {"xx-small", "x-small", "small", "medium", "large", "x-large", "xx-large", "xxx-large"};
  const double sizes[] = {9, 10, 13, 16, 18, 24, 32, 48};
  for (size_t i = 0; i < 8; ++i) if (size == keywords[i]) number = {CssLength, sizes[i]};
  if (!style.font_computed && (number.kind == CssLength || (number.kind == CssNumber && number.number == 0))) {
    // Gecko specified/font.rs::quantize_font_size: retain ten significant bits.
    const float scale = 16385.f, limit = std::numeric_limits<float>::max() / scale;
    const float input = float(std::max(0.0, number.number));
    volatile float product = input * scale;
    volatile float tail = product - input;
    const float pixels = input >= limit ? limit : product - tail;
    style.computed_font_size = pixels; style.font_computed = true;
    style.set_property("font-size", CssFontNumber(pixels) + "px");
  }
  const double pixels = FontPixels(style);
  auto height = CssLower(FontValue(style, "line-height"));
  if (height != "normal" && !style.line_height_computed) {
    auto value = CssNumericResolve(height, pixels, root_size, pixels);
    if (value.kind == CssLength) {
      style.computed_line_height = float(value.number); style.line_height_computed = true;
      style.set_property("line-height", CssFontNumber(value.number) + "px");
    }
    else if (value.kind == CssNumber) style.set_property("line-height", CssFontNumber(value.number));
  }
  auto weight = CssLower(FontValue(style, "font-weight"));
  if (weight == "normal") weight = "400";
  if (weight == "bold") weight = "700";
  if (weight == "bolder" || weight == "lighter") {
    auto value = CssNumericParse(FontValue(parent, "font-weight"));
    double n = value.kind == CssNumber ? value.number : 400;
    weight = weight == "bolder" ? (n < 350 ? "400" : n < 550 ? "700" : "900") :
                                (n < 550 ? "100" : n < 750 ? "400" : "700");
  }
  style.set_property("font-weight", weight);
  auto stretch = CssLower(FontValue(style, "font-stretch"));
  const char* widths[] = {"ultra-condensed", "extra-condensed", "condensed", "semi-condensed",
    "normal", "semi-expanded", "expanded", "extra-expanded", "ultra-expanded"};
  const char* percentages[] = {"50%", "62.5%", "75%", "87.5%", "100%", "112.5%", "125%", "150%", "200%"};
  for (size_t i = 0; i < 9; ++i) if (stretch == widths[i]) stretch = percentages[i];
  style.set_property("font-stretch", stretch);
}
std::string CssResolvedLineHeight(const StyleDeclaration& style) {
  auto text = FontValue(style, "line-height");
  auto number = CssNumericParse(text);
  if (number.kind == CssNumber || style.line_height_computed) {
    const float height = number.kind == CssNumber ? float(number.number) * float(FontPixels(style)) :
                                                                   float(style.computed_line_height);
    return CssFontNumber(float(std::floor(height * 60.f + .5f)) / 60.f) + "px";
  }
  return text;
}
}  // namespace ruyijs::domain::styling
