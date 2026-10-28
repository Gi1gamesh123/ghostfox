#include "domain/styling/css_box_value.h"
#include "domain/styling/css_flex_value.h"
#include "domain/styling/css_overflow_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_color_value.h"
#include "domain/styling/css_numeric_value.h"
#include <array>
namespace ruyijs::domain::styling {
namespace {
constexpr const char* BoxSides[] = {"top", "right", "bottom", "left"};
bool BoxWord(const std::string& value, const char* words) {
  std::string_view remaining(words);
  while (!remaining.empty()) {
    const auto end = remaining.find(',');
    if (remaining.substr(0,end) == value) return true;
    if (end == remaining.npos) break;
    remaining.remove_prefix(end+1);
  }
  return false;
}
bool BoxWide(const std::string& value) {
  return BoxWord(value, "initial,inherit,unset,revert,revert-layer");
}
std::vector<std::string> BoxTokens(const std::string& text) {
  std::vector<std::string> result;
  for (size_t p = 0; p < text.size();) {
    p = CssSkip(text, p);
    if (p == text.size()) break;
    const auto end = CssDelimiter(text, p, " \t\r\n\f");
    result.push_back(text.substr(p, end-p)); p = end;
    if (result.size() > 4) break;
  }
  return result;
}
bool BoxWidth(const std::string& value) {
  return BoxWord(value, "thin,medium,thick") || CssNumericAllowed(value, CssLength, false);
}
bool BoxStyle(const std::string& value) {
  return BoxWord(value, "none,hidden,dotted,dashed,solid,double,groove,ridge,inset,outset");
}
std::string BoxLength(std::string value) {
  const auto number = CssNumericParse(value);
  return number.kind == CssNumber && number.number == 0 && !number.expression ? "0px" : value;
}
}
bool CssBoxShorthand(std::string_view name) {
  if (CssFlexShorthand(name) || CssOverflowShorthand(name)) return true;
  if (name == "margin" || name == "padding" || name == "inset" || name == "border" ||
      name == "border-width" || name == "border-style" || name == "border-color") return true;
  return name == "border-top" || name == "border-right" ||
         name == "border-bottom" || name == "border-left";
}
std::vector<std::string> CssBoxLonghands(std::string_view name) {
  std::vector<std::string> result;
  if (CssFlexShorthand(name)) return CssFlexLonghands(name);
  if (CssOverflowShorthand(name)) return {"overflow-x", "overflow-y"};
  if (!CssBoxShorthand(name)) return result;
  for (auto side : BoxSides) {
    const auto prefix = std::string("border-")+side;
    if (name == "margin" || name == "padding") result.push_back(std::string(name)+"-"+side);
    else if (name == "inset") result.push_back(side);
    else if (name == "border" || name == prefix)
      for (auto suffix : {"-width", "-style", "-color"}) result.push_back(prefix+suffix);
    else if (name == "border-width" || name == "border-style" || name == "border-color")
      result.push_back(prefix+std::string(name.substr(6)));
  }
  if (name == "border") for (auto field : {"source", "slice", "width", "outset", "repeat"})
    result.push_back(std::string("border-image-")+field);
  return result;
}
bool CssBoxExpand(const std::string& name, const std::string& input, bool important,
                  std::vector<CssDeclarationValue>* out) {
  if (CssFlexShorthand(name)) return CssFlexExpand(name, input, important, out);
  if (CssOverflowShorthand(name)) return CssOverflowExpand(input, important, out);
  const auto names = CssBoxLonghands(name);
  if (names.empty() || !CssValueComponentsValid(input)) return false;
  std::string text;
  for (size_t p = 0; p < input.size();) {
    if (input.compare(p,2,"/*") == 0) { p = CssSkip(input,p); text += ' '; }
    else text += input[p++];
  }
  const auto value = CssLower(CssTrim(text));
  if (BoxWide(value)) {
    for (const auto& key : names) out->push_back({key, value, important});
    return true;
  }
  // Deferred substitutions retain their original expression in the one store.
  if (value.find("var(") != value.npos || value.find("env(") != value.npos) {
    if (!CssVariableReferenceValid(input)) return false;
    out->push_back({name, input, important}); return true;
  }
  const auto tokens = BoxTokens(value);
  if (tokens.empty() || tokens.size() > 4) return false;
  for (const auto& token : tokens) if (BoxWide(token)) return false;
  std::vector<CssDeclarationValue> result;
  if (names.size() == 4) {
    for (const auto& token : tokens) {
      const bool valid = name == "border-width" ? BoxWidth(token) :
          name == "border-style" ? BoxStyle(token) : name == "border-color" ? CssColorSupported(token) :
          ((name == "margin" || name == "inset") && token == "auto") ||
          CssNumericAllowed(token, CssLength|CssPercent, name != "padding");
      if (!valid) return false;
    }
    const size_t indices[] = {0, tokens.size()>1?1u:0u, tokens.size()>2?2u:0u,
                             tokens.size()>3?3u:tokens.size()>1?1u:0u};
    for (size_t i = 0; i < 4; ++i)
      result.push_back({names[i], BoxLength(tokens[indices[i]]), important});
  } else {
    std::array<std::string,3> values = {"medium", "none", "currentcolor"};
    bool seen[3] = {};
    if (tokens.size() > 3) return false;
    for (const auto& token : tokens) {
      const int kind = BoxWidth(token) ? 0 : BoxStyle(token) ? 1 : CssColorSupported(token) ? 2 : -1;
      if (kind < 0 || seen[kind]) return false;
      seen[kind] = true; values[kind] = kind == 0 ? BoxLength(token) : token;
    }
    for (size_t i = 0; i < (name == "border" ? 12u : 3u); ++i)
      result.push_back({names[i], values[i%3], important});
    if (name == "border") {
      constexpr const char* defaults[] = {"none", "100%", "1", "0", "stretch"};
      for (size_t i = 12; i < names.size(); ++i) result.push_back({names[i], defaults[i-12], important});
    }
  }
  *out = std::move(result); return true;
}
}
