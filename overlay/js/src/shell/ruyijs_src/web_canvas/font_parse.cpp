// The 2D context's font shorthand. Firefox parses ctx.font as the CSS `font`
// property and keeps the whole font; text measurement only needs the size, the
// weight, the slant and the first family of the list, which is what this reads.
// A string this parser cannot accept leaves the caller on the previous font.
#include "web_canvas/internal.h"

#include <cmath>
#include <cstdlib>

namespace ruyijs::web_canvas::detail {
namespace {
bool IsSpace(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f';
}

std::string Trim(const std::string& text) {
  size_t begin = 0, end = text.size();
  while (begin < end && IsSpace(text[begin])) ++begin;
  while (end > begin && IsSpace(text[end - 1])) --end;
  return text.substr(begin, end - begin);
}

bool Equals(const std::string& text, const char* word) {
  size_t index = 0;
  for (; index < text.size(); ++index) {
    char left = text[index];
    if (left >= 'A' && left <= 'Z') left = char(left - 'A' + 'a');
    if (word[index] == 0 || left != word[index]) return false;
  }
  return word[index] == 0;
}

// The absolute CSS size keywords, in whole CSS pixels.
bool KeywordSize(const std::string& word, double* size) {
  static const struct Entry { const char* name; double size; } kSizes[] = {
      {"xx-small", 9}, {"x-small", 10}, {"small", 13}, {"medium", 16},
      {"large", 18}, {"x-large", 24}, {"xx-large", 32}, {"xxx-large", 48}};
  for (const auto& entry : kSizes)
    if (Equals(word, entry.name)) { *size = entry.size; return true; }
  return false;
}

// Keywords accepted before the size. Only weight and slant change measurement.
bool FontKeyword(const std::string& word, CanvasFont* out) {
  if (Equals(word, "italic") || Equals(word, "oblique")) {
    out->italic = true;
  } else if (Equals(word, "bold") || Equals(word, "bolder")) {
    out->weight = 700;
  } else if (Equals(word, "lighter")) {
    out->weight = 100;
  } else if (word.size() == 3 && word[0] >= '1' && word[0] <= '9' &&
             word[1] >= '0' && word[1] <= '9' && word[2] == '0') {
    out->weight = unsigned(std::atoi(word.c_str()));
  } else if (!(Equals(word, "normal") || Equals(word, "small-caps") ||
               Equals(word, "ultra-condensed") || Equals(word, "extra-condensed") ||
               Equals(word, "condensed") || Equals(word, "semi-condensed") ||
               Equals(word, "semi-expanded") || Equals(word, "expanded") ||
               Equals(word, "extra-expanded") || Equals(word, "ultra-expanded"))) {
    return false;
  }
  return true;
}

// A CSS length in whole pixels; a unitless number is not a font size.
bool LengthPixels(const std::string& text, double* out) {
  if (text.empty()) return false;
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || !std::isfinite(value)) return false;
  const std::string unit = Trim(std::string(end));
  double factor = 0;
  if (Equals(unit, "px")) factor = 1;
  else if (Equals(unit, "pt")) factor = 96.0 / 72.0;
  else if (Equals(unit, "pc")) factor = 16;
  else if (Equals(unit, "in")) factor = 96;
  else if (Equals(unit, "cm")) factor = 96 / 2.54;
  else if (Equals(unit, "mm")) factor = 96 / 25.4;
  else if (Equals(unit, "q")) factor = 96 / 101.6;
  else if (Equals(unit, "em") || Equals(unit, "rem")) factor = 16;
  else if (Equals(unit, "ex") || Equals(unit, "ch")) factor = 8;
  if (factor == 0) return false;
  *out = value * factor;
  return *out > 0;
}

// Firefox quantizes the canvas font size to seven significant bits, so its font
// caches do not fill with imperceptibly different sizes (QuantizeFontSize in
// CanvasRenderingContext2D.cpp, the Veltkamp-Dekker split). Repeating the same
// float steps is what makes a measured advance match Firefox's.
float QuantizeFontSize(float size) {
  constexpr int kBitsToDrop = 17;  // leaving 7 bits of precision
  constexpr int kScale = 1 << kBitsToDrop;
  const float wide = size * float(kScale + 1);
  const float difference = wide - size;
  return wide - difference;
}

// The first family of the list, with quotes and the rest of the list removed.
std::string FirstFamily(const std::string& list) {
  std::string family;
  bool quoted = false;
  char quote = 0;
  for (size_t index = 0; index < list.size(); ++index) {
    const char ch = list[index];
    if (quoted) {
      if (ch == quote) quoted = false;
      else family.push_back(ch);
      continue;
    }
    if (ch == '"' || ch == '\'') { quoted = true; quote = ch; continue; }
    if (ch == ',') break;
    family.push_back(ch);
  }
  return Trim(family);
}
}  // namespace

bool ParseCanvasFont(const std::string& shorthand, CanvasFont* out) {
  *out = CanvasFont();
  const std::string text = Trim(shorthand);
  size_t at = 0;
  while (at < text.size()) {
    while (at < text.size() && IsSpace(text[at])) ++at;
    size_t end = at;
    while (end < text.size() && !IsSpace(text[end])) ++end;
    const std::string word = text.substr(at, end - at);
    if (word.empty()) break;
    // The size, optionally followed by "/line-height", ends the prelude.
    const size_t slash = word.find('/');
    const std::string length = slash == std::string::npos ? word : word.substr(0, slash);
    if (KeywordSize(length, &out->size) || LengthPixels(length, &out->size)) {
      at = end;
      break;
    }
    if (!FontKeyword(word, out)) return false;
    at = end;
  }
  const std::string family = FirstFamily(Trim(text.substr(at)));
  if (family.empty()) return false;
  out->family = family;
  out->size = QuantizeFontSize(float(out->size));
  out->valid = true;
  return true;
}
}  // namespace ruyijs::web_canvas::detail
