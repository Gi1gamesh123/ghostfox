// Reconstruct font from one declaration set; no independent shorthand state.
#include "domain/styling/css_font_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_float_text.h"
namespace ruyijs::domain::styling {
std::string CssFontNumber(double value) {
  return CssFloatText(static_cast<float>(value));
}
std::string CssFontSerialize(const std::vector<CssDeclarationValue>& rows, bool* important) {
  std::vector<std::string> values;
  bool priority = false;
  for (const auto& property : CssFontDefaults()) {
    const CssDeclarationValue* found = nullptr;
    for (const auto& row : rows) if (row.name == property.name) { found = &row; break; }
    if (!found) return {};
    if (values.empty()) priority = found->important;
    else if (priority != found->important) return {};
    values.push_back(found->value);
  }
  // A system font shorthand serializes back to its keyword: the longhands are
  // the platform's system font, not values the declaration spelled out.
  if (CssSystemFontKeyword(values[6])) {
    bool untouched = true;
    for (size_t i = 0; i < values.size(); ++i)
      if (i != 6 && values[i] != CssFontDefaults()[i].value) untouched = false;
    if (untouched) {
      if (important) *important = priority;
      return values[6];
    }
  }
  bool same = true;
  for (const auto& value : values) if (value != values[0]) same = false;
  if (same && (values[0] == "inherit" || values[0] == "initial" || values[0] == "unset" ||
               values[0] == "revert" || values[0] == "revert-layer")) {
    if (important) *important = priority;
    return values[0];
  }
  for (size_t i = 7; i < values.size(); ++i)
    if (values[i] != CssFontDefaults()[i].value) return {};
  if (values[1] != "normal" && values[1] != "small-caps") return {};
  const char* stretch[] = {"ultra-condensed", "extra-condensed", "condensed", "semi-condensed",
    "normal", "semi-expanded", "expanded", "extra-expanded", "ultra-expanded"};
  const char* widths[] = {"50%", "62.5%", "75%", "87.5%", "100%", "112.5%", "125%", "150%", "200%"};
  for (size_t i = 0; i < 9; ++i) if (values[3] == widths[i]) values[3] = stretch[i];
  if (values[2] == "400") values[2] = "normal";
  std::string result;
  for (size_t i = 0; i < 4; ++i) if (values[i] != "normal") result += values[i] + " ";
  result += values[4];
  if (values[5] != "normal") result += " / " + values[5];
  result += " " + values[6];
  if (important) *important = priority;
  return result;
}
}  // namespace ruyijs::domain::styling
