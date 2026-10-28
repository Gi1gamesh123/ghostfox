// The computed value of a <color>: Firefox serializes every resolved sRGB
// colour as rgb(r, g, b) or rgba(r, g, b, a) whatever its specified form
// (names, hex, rgb()/hsl() syntaxes, transparent, currentcolor).
#include "domain/styling/css_color_value.h"
#include <cmath>
#include <cstdio>
#include <string>

namespace ruyijs::domain::styling {
namespace {
std::string Lower(std::string_view text) {
  std::string out(text);
  for (char& c : out) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return out;
}
std::string Trim(std::string_view text) {
  size_t a = 0, b = text.size();
  while (a < b && (text[a] == ' ' || text[a] == '\t' || text[a] == '\n')) ++a;
  while (b > a && (text[b - 1] == ' ' || text[b - 1] == '\t' || text[b - 1] == '\n')) --b;
  return std::string(text.substr(a, b - a));
}
// CSS Color 4 serialization: the alpha of an 8-bit colour with two decimals,
// or three when two do not round-trip.
std::string Alpha(uint8_t a) {
  char buffer[16];
  for (int digits : {2, 3}) {
    const double scale = digits == 2 ? 100.0 : 1000.0;
    const double value = std::round(a / 255.0 * scale) / scale;
    if (digits == 3 || std::lround(value * 255.0) == a) {
      std::snprintf(buffer, sizeof buffer, digits == 2 ? "%.2f" : "%.3f", value);
      std::string text(buffer);
      while (!text.empty() && text.back() == '0') text.pop_back();
      if (!text.empty() && text.back() == '.') text.pop_back();
      return text;
    }
  }
  return "0";
}
}  // namespace
std::string CssComputedColor(std::string_view value, std::string_view current_color) {
  const std::string text = Trim(value);
  if (text.empty()) return {};
  const std::string lower = Lower(text);
  if (lower == "currentcolor") return current_color.empty() ? std::string() : CssComputedColor(current_color, {});
  uint8_t rgba[4];
  if (!CssColorRgba(text, rgba)) return {};
  char buffer[64];
  if (rgba[3] == 255) std::snprintf(buffer, sizeof buffer, "rgb(%u, %u, %u)", rgba[0], rgba[1], rgba[2]);
  else std::snprintf(buffer, sizeof buffer, "rgba(%u, %u, %u, %s)", rgba[0], rgba[1], rgba[2], Alpha(rgba[3]).c_str());
  return buffer;
}
}  // namespace ruyijs::domain::styling
