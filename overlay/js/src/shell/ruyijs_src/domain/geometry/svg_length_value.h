// Pure SVG length values. Live ownership belongs to the original Element.
#pragma once
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace ruyijs::domain::geometry {
struct SvgLengthValue {
  float number = 0;
  uint16_t unit = 1;
  bool specified = false;
};
inline constexpr const char* kSvgLengthUnits[] = {
  "", "", "%", "em", "ex", "px", "cm", "mm", "in", "pt", "pc", "q", "ch",
  "rem", "ic", "cap", "lh", "rlh", "rex", "rch", "ric", "rcap", "vw", "vh", "vmin", "vmax"
};
inline bool SvgLengthSpace(char16_t c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
inline bool SvgLengthDigit(char16_t c) { return c >= '0' && c <= '9'; }
inline bool ParseSvgLength(std::u16string_view text, SvgLengthValue* out, bool trim = false) {
  if (trim) {
    while (!text.empty() && SvgLengthSpace(text.front())) text.remove_prefix(1);
    while (!text.empty() && SvgLengthSpace(text.back())) text.remove_suffix(1);
  }
  if (text.empty()) return false;
  size_t p = 0;
  if (text[p] == '+' || text[p] == '-') ++p;
  size_t first = p;
  while (p < text.size() && SvgLengthDigit(text[p])) ++p;
  bool digits = p > first;
  if (p < text.size() && text[p] == '.') {
    first = ++p;
    while (p < text.size() && SvgLengthDigit(text[p])) ++p;
    if (p == first) return false;
    digits = true;
  }
  if (!digits) return false;
  // An 'e' followed by m/x is a unit, not a number exponent.
  if (p + 1 < text.size() && (text[p] == 'e' || text[p] == 'E') &&
      (SvgLengthDigit(text[p + 1]) || text[p + 1] == '+' || text[p + 1] == '-')) {
    ++p;
    if (text[p] == '+' || text[p] == '-') ++p;
    first = p;
    while (p < text.size() && SvgLengthDigit(text[p])) ++p;
    if (p == first) return false;
  }
  std::string number, suffix;
  for (size_t i = 0; i < p; ++i) number += char(text[i]);
  for (size_t i = p; i < text.size(); ++i) {
    char16_t c = text[i];
    if (c > 127) return false;
    suffix += char(c >= 'A' && c <= 'Z' ? c + 32 : c);
  }
  unsigned unit = 0;
  for (unsigned i = 1; i < 26; ++i) if (suffix == kSvgLengthUnits[i]) { unit = i; break; }
  if (!unit) return false;
  char* end = nullptr;
  const double parsed = std::strtod(number.c_str(), &end);
  const float value = float(parsed);
  if (!end || *end || !std::isfinite(value)) return false;
  *out = {value, uint16_t(unit), true};
  return true;
}
inline std::string SerializeSvgLength(const SvgLengthValue& value) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%g", double(value.number));
  return std::string(buffer) + kSvgLengthUnits[value.unit];
}
inline bool SvgLengthAbsolute(unsigned unit) { return unit == 1 || (unit >= 5 && unit <= 11); }
inline float SvgLengthAbsoluteScale(unsigned target, unsigned source) {
  // Float32 constants and direct conversion avoid double-rounding via pixels.
  static constexpr float table[7][7] = {
    {1.f,37.7952755906f,3.779528f,96.f,1.33333333333333333f,16.f,.94488188988f},
    {.02645833333f,1.f,.1f,2.54f,.035277777777777778f,.42333333333333333f,.025f},
    {.26458333333f,10.f,1.f,25.4f,.35277777777777778f,4.2333333333333333f,.25f},
    {.01041666666f,.39370078740157481f,.039370078740157481f,1.f,.013888888888888889f,.16666666666666667f,.02204860853f},
    {.75f,28.346456692913386f,2.8346456692913386f,72.f,1.f,12.f,.70866141732f},
    {.0625f,2.3622047244094489f,.23622047244094489f,6.f,.083333333333333333f,1.f,16.9333333333f},
    {1.0583333332f,40.f,4.f,45.354336f,1.41111111111f,16.9333333333f,1.f}
  };
  return table[target == 1 ? 0 : target - 5][source == 1 ? 0 : source - 5];
}
}
