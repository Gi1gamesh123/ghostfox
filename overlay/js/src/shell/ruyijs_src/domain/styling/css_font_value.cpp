// Expand before duplicate elimination so font and its longhands cascade in order.
#include "domain/styling/css_font_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_condition_value.h"
#include "domain/styling/css_numeric_value.h"
#include <algorithm>
#include <cmath>
namespace ruyijs::domain::styling {
namespace {
std::string& InitialFamily() { static std::string family = "serif"; return family; }
std::vector<CssDeclarationValue>& Defaults();
}
void CssSetInitialFontFamily(std::string generic) {
  if (generic.empty()) return;
  InitialFamily() = std::move(generic);
  for (auto& row : Defaults()) if (row.name == "font-family") row.value = InitialFamily();
}
const std::string& CssInitialFontFamily() { return InitialFamily(); }
const std::vector<CssDeclarationValue>& CssFontDefaults() { return Defaults(); }
namespace {
std::vector<CssDeclarationValue>& Defaults() {
  static std::vector<CssDeclarationValue> values = {
    {"font-style", "normal"}, {"font-variant-caps", "normal"},
    {"font-weight", "normal"}, {"font-stretch", "normal"},
    {"font-size", "medium"}, {"line-height", "normal"}, {"font-family", CssInitialFontFamily()},
    {"font-size-adjust", "none"}, {"font-kerning", "auto"},
    {"font-optical-sizing", "auto"}, {"font-feature-settings", "normal"},
    {"font-variation-settings", "normal"}, {"font-language-override", "normal"},
    {"font-variant-ligatures", "normal"}, {"font-variant-numeric", "normal"},
    {"font-variant-east-asian", "normal"}, {"font-variant-position", "normal"},
    {"font-variant-alternates", "normal"}, {"font-variant-emoji", "normal"}};
  return values;
}
}
namespace {
std::string FontToken(const std::string& text, size_t* p) {
  *p = CssSkip(text, *p);
  const auto end = CssDelimiter(text, *p, " \t\r\n\f/");
  auto result = text.substr(*p, end - *p); *p = end;
  return result;
}
bool FontKeyword(const std::string& token, const char* list) {
  for (const auto& word : CssSplit(list, ',')) if (token == word) return true;
  return false;
}
}
bool CssSystemFontKeyword(const std::string& value) {
  return FontKeyword(value,
      "caption,icon,menu,message-box,small-caption,status-bar");
}
bool CssFontExpand(const std::string& input, bool important, std::vector<CssDeclarationValue>* out) {
  // Comments separate tokens even where no whitespace was present.
  std::string text;
  char quote = 0;
  for (size_t p = 0; p < input.size();) {
    char c = input[p++];
    if (c == '\\') { text += c; if (p < input.size()) text += input[p++]; continue; }
    if (quote) { text += c; if (c == quote) quote = 0; continue; }
    if (c == '\'' || c == '"') { quote = c; text += c; continue; }
    if (c == '/' && p < input.size() && input[p] == '*') {
      p = CssSkip(input, p - 1); text += ' ';
    } else text += c;
  }
  text = CssTrim(text);
  auto values = CssFontDefaults();
  for (auto& value : values) value.important = important;
  const auto lower = CssLower(text);
  if (FontKeyword(lower, "inherit,initial,unset,revert,revert-layer")) {
    for (auto& value : values) value.value = lower;
    *out = std::move(values); return true;
  }
  // A system font keyword is a whole shorthand value, not a size: it sets every
  // font longhand to the platform's system font. The family keeps the keyword,
  // so the shorthand serializes back to it and fontFamily stays unset (Firefox).
  if (CssSystemFontKeyword(lower)) {
    values[6].value = lower;
    *out = std::move(values); return true;
  }
  bool seen[4] = {}; unsigned normals = 0;
  size_t p = 0;
  bool sized = false;
  while (p < text.size()) {
    auto token = CssLower(FontToken(text, &p));
    if (FontKeyword(token, "inherit,initial,unset,revert,revert-layer")) return false;
    if (CssSupportsProperty("font-size", token)) { values[4].value = token; sized = true; break; }
    if (token == "normal") { if (++normals > 4) return false; continue; }
    int field = -1;
    if (token == "italic" || token == "oblique") field = 0;
    else if (token == "small-caps") field = 1;
    else if (CssSupportsProperty("font-weight", token)) field = 2;
    else if (FontKeyword(token, "ultra-condensed,extra-condensed,condensed,semi-condensed,semi-expanded,expanded,extra-expanded,ultra-expanded")) field = 3;
    if (field < 0 || seen[field]) return false;
    seen[field] = true;
    if (token == "oblique") {
      auto after = p; const auto angle = CssLower(FontToken(text, &after));
      auto number = CssNumericParse(angle);
      if (number.kind == CssAngle && !number.expression) {
        size_t unit = 0; double amount = 0; CssNumberToken(angle, &unit, &amount);
        const auto suffix = angle.substr(unit);
        if (suffix == "rad") amount *= 180 / 3.14159265358979323846;
        if (suffix == "grad") amount *= .9;
        if (suffix == "turn") amount *= 360;
        if (std::abs(amount) > 90) return false;
        token += " " + CssFontNumber(amount) + "deg"; p = after;
      }
    }
    values[field].value = token;
  }
  if (!sized || normals + std::count(std::begin(seen), std::end(seen), true) > 4) return false;
  p = CssSkip(text, p);
  if (p < text.size() && text[p] == '/') {
    ++p; auto height = CssLower(FontToken(text, &p));
    if (FontKeyword(height, "inherit,initial,unset,revert,revert-layer")) return false;
    if (!CssSupportsProperty("line-height", height)) return false;
    values[5].value = height;
  }
  values[6].value = CssFontFamily(std::string_view(text).substr(p));
  if (values[6].value.empty()) return false;
  *out = std::move(values); return true;
}
}  // namespace ruyijs::domain::styling
