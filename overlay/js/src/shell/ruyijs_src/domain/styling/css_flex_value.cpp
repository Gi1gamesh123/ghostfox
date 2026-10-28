// The flex, flex-flow and gap shorthands (CSS Flexbox 1 §7.1, §5.3, CSS Box
// Alignment §8.3): expansion into their longhands and serialization back, as
// Firefox 158 does. They take the box shorthand dispatch in css_box_value.cpp,
// so declarations, CSSOM and @supports need no second path.
#include "domain/styling/css_flex_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_numeric_value.h"
#include "domain/styling/css_color_value.h"
#include <cstdio>
#include <algorithm>
namespace ruyijs::domain::styling {
namespace {
bool Wide(const std::string& v) {
  return v == "initial" || v == "inherit" || v == "unset" || v == "revert" || v == "revert-layer";
}
std::vector<std::string> Tokens(const std::string& text) {
  std::vector<std::string> out;
  for (size_t p = 0; p < text.size();) {
    p = CssSkip(text, p);
    if (p == text.size()) break;
    const auto end = CssDelimiter(text, p, " \t\r\n\f");
    out.push_back(text.substr(p, end - p));
    p = end;
    if (out.size() > 3) break;
  }
  return out;
}
bool Factor(const std::string& v) {
  const auto n = CssNumericParse(v);
  return CssNumericAllowed(v, CssNumber, false) && !(n.kind == CssNumber && !n.expression && n.number < 0);
}
bool Basis(const std::string& v) {
  return v == "auto" || v == "content" || v == "max-content" || v == "min-content" || v == "fit-content" ||
         CssNumericAllowed(v, CssLength | CssPercent, false);
}
// A unitless zero basis is "0px" (Firefox serializes the computed length).
std::string BasisText(const std::string& v) {
  const auto n = CssNumericParse(v);
  return n.kind == CssNumber && !n.expression && n.number == 0 ? "0px" : v;
}
// A plain number in its shortest form (Firefox serializes "1.0" as "1"); calc() stays as written.
std::string Number(const std::string& v) {
  const auto n = CssNumericParse(v);
  if (n.kind != CssNumber || n.expression) return v;
  char text[32];
  std::snprintf(text, sizeof text, "%.6g", n.number);
  return text;
}
bool Direction(const std::string& v) { return v == "row" || v == "row-reverse" || v == "column" || v == "column-reverse"; }
bool Wrap(const std::string& v) { return v == "nowrap" || v == "wrap" || v == "wrap-reverse"; }
bool Gap(const std::string& v) { return v == "normal" || CssNumericAllowed(v, CssLength | CssPercent, false); }
std::string GapText(const std::string& v) {
  const auto n = CssNumericParse(v);
  return n.kind == CssNumber && !n.expression && n.number == 0 ? "0px" : v;
}
}  // namespace

bool CssFlexShorthand(std::string_view name) { return name == "flex" || name == "flex-flow" || name == "gap"; }
std::vector<std::string> CssFlexLonghands(std::string_view name) {
  if (name == "flex") return {"flex-grow", "flex-shrink", "flex-basis"};
  if (name == "flex-flow") return {"flex-direction", "flex-wrap"};
  if (name == "gap") return {"row-gap", "column-gap"};
  return {};
}
bool CssFlexExpand(const std::string& name, const std::string& input, bool important,
                   std::vector<CssDeclarationValue>* out) {
  const auto names = CssFlexLonghands(name);
  if (names.empty() || !CssValueComponentsValid(input)) return false;
  const auto value = CssLower(CssTrim(CssClean(input)));
  if (Wide(value)) {
    for (const auto& key : names) out->push_back({key, value, important});
    return true;
  }
  if (value.find("var(") != value.npos || value.find("env(") != value.npos) {
    if (!CssVariableReferenceValid(input)) return false;
    out->push_back({name, input, important});
    return true;
  }
  const auto tokens = Tokens(value);
  if (tokens.empty()) return false;
  for (const auto& t : tokens) if (Wide(t)) return false;
  std::vector<std::string> values;
  if (name == "flex") {
    if (tokens.size() > 3) return false;
    if (tokens.size() == 1 && tokens[0] == "none") values = {"0", "0", "auto"};
    else if (tokens.size() == 1 && tokens[0] == "auto") values = {"1", "1", "auto"};
    else {
      // <flex-grow> <flex-shrink>? || <flex-basis>; an omitted basis is 0%.
      std::string grow, shrink, basis;
      size_t i = 0;
      bool numbers = false;
      if (i < tokens.size() && Basis(tokens[i]) && !Factor(tokens[i])) basis = tokens[i++];
      if (i < tokens.size() && Factor(tokens[i])) { grow = tokens[i++]; numbers = true; }
      if (numbers && i < tokens.size() && Factor(tokens[i])) shrink = tokens[i++];
      if (basis.empty() && i < tokens.size() && Basis(tokens[i])) basis = tokens[i++];
      if (i != tokens.size() || (grow.empty() && basis.empty())) return false;
      values = {grow.empty() ? "1" : Number(grow), shrink.empty() ? "1" : Number(shrink),
                basis.empty() ? "0%" : BasisText(basis)};
    }
  } else if (name == "flex-flow") {
    if (tokens.size() > 2) return false;
    std::string direction, wrap;
    for (const auto& t : tokens) {
      if (Direction(t) && direction.empty()) direction = t;
      else if (Wrap(t) && wrap.empty()) wrap = t;
      else return false;
    }
    values = {direction.empty() ? "row" : direction, wrap.empty() ? "nowrap" : wrap};
  } else {
    if (tokens.size() > 2) return false;
    for (const auto& t : tokens) if (!Gap(t)) return false;
    values = {GapText(tokens[0]), GapText(tokens.size() > 1 ? tokens[1] : tokens[0])};
  }
  for (size_t i = 0; i < names.size(); ++i) out->push_back({names[i], values[i], important});
  return true;
}
std::string CssFlexSerialize(const std::vector<CssDeclarationValue>& rows, const std::string& name,
                             bool* important) {
  const auto names = CssFlexLonghands(name);
  if (important) *important = false;
  if (names.empty()) return {};
  // A var()-pending shorthand stands for its longhands only while none of
  // them was declared after it.
  auto pending = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.name == name; });
  if (pending != rows.end() && std::none_of(rows.begin(), rows.end(), [&](const auto& row) {
        return std::find(names.begin(), names.end(), row.name) != names.end(); })) {
    if (important) *important = pending->important;
    return pending->value;
  }
  std::vector<std::string> values;
  bool flag = false;
  for (const auto& key : names) {
    auto entry = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.name == key; });
    if (entry == rows.end() || (!values.empty() && entry->important != flag)) return {};
    flag = entry->important;
    values.push_back(entry->value);
  }
  if (important) *important = flag;
  const bool allWide = std::all_of(values.begin(), values.end(), [&](const auto& v) { return v == values[0]; }) && Wide(values[0]);
  if (allWide) return values[0];
  if (std::any_of(values.begin(), values.end(), Wide)) return {};
  if (name == "gap") return values[0] == values[1] ? values[0] : values[0] + " " + values[1];
  if (name == "flex-flow") {
    if (values[1] == "nowrap") return values[0];
    return values[0] == "row" ? values[1] : values[0] + " " + values[1];
  }
  return values[0] + " " + values[1] + " " + values[2];
}
}  // namespace ruyijs::domain::styling
