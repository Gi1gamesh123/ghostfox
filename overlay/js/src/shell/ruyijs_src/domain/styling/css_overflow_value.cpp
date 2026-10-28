// The overflow shorthand (CSS Overflow 3 §3): overflow-x then overflow-y, a
// single value setting both; and the computed-value rule that a visible/clip
// axis beside a scrollable one computes to auto/hidden. Dispatched from the box
// shorthand entry points (css_box_value.cpp) like flex.
#include "domain/styling/css_overflow_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_color_value.h"
#include "domain/styling/stylesheet.h"

namespace ruyijs::domain::styling {
namespace {
bool OverflowWide(const std::string& v) {
  return v == "initial" || v == "inherit" || v == "unset" || v == "revert" || v == "revert-layer";
}
bool OverflowWord(const std::string& v) {
  return v == "visible" || v == "hidden" || v == "clip" || v == "scroll" || v == "auto";
}
}  // namespace

bool CssOverflowShorthand(std::string_view name) { return name == "overflow"; }
bool CssOverflowLonghand(std::string_view name) { return name == "overflow-x" || name == "overflow-y"; }
bool CssOverflowKeyword(const std::string& value) { return OverflowWord(value); }

bool CssOverflowExpand(const std::string& input, bool important, std::vector<CssDeclarationValue>* out) {
  if (!CssValueComponentsValid(input)) return false;
  const auto value = CssLower(CssTrim(CssClean(input)));
  if (OverflowWide(value)) {
    out->push_back({"overflow-x", value, important});
    out->push_back({"overflow-y", value, important});
    return true;
  }
  if (value.find("var(") != value.npos || value.find("env(") != value.npos) {
    if (!CssVariableReferenceValid(input)) return false;
    out->push_back({"overflow", input, important});
    return true;
  }
  const auto parts = CssSplit(value, ' ');
  std::vector<std::string> words;
  for (const auto& part : parts) if (!part.empty()) words.push_back(part);
  if (words.empty() || words.size() > 2) return false;
  for (const auto& word : words) if (!OverflowWord(word)) return false;
  out->push_back({"overflow-x", words[0], important});
  out->push_back({"overflow-y", words.size() > 1 ? words[1] : words[0], important});
  return true;
}

std::string CssOverflowSerialize(const std::vector<CssDeclarationValue>& rows, bool* important) {
  if (important) *important = false;
  const CssDeclarationValue *x = nullptr, *y = nullptr, *pending = nullptr;
  for (const auto& row : rows) {
    if (row.name == "overflow-x") x = &row;
    else if (row.name == "overflow-y") y = &row;
    else if (row.name == "overflow") pending = &row;
  }
  if (pending && !x && !y) {
    if (important) *important = pending->important;
    return pending->value;
  }
  if (!x || !y || x->important != y->important) return {};
  if (important) *important = x->important;
  if (x->value == y->value) return x->value;
  if (OverflowWide(x->value) || OverflowWide(y->value)) return {};
  return x->value + " " + y->value;
}

void CssComputeOverflow(StyleDeclaration& style) {
  auto x = style.get_property_value("overflow-x"), y = style.get_property_value("overflow-y");
  if (x.empty()) x = "visible";
  if (y.empty()) y = "visible";
  const auto passive = [](const std::string& v) { return v == "visible" || v == "clip"; };
  if (passive(x) == passive(y)) return;
  const auto fix = [](const std::string& v) { return v == "visible" ? "auto" : v == "clip" ? "hidden" : v; };
  if (passive(x)) style.set_property("overflow-x", fix(x));
  else style.set_property("overflow-y", fix(y));
}
}  // namespace ruyijs::domain::styling
