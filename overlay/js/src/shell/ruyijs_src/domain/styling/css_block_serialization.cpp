// CSSOM §6.7.2 serialize a CSS declaration block, and the shorthand-aware
// getPropertyValue lookup. Every declaration block (rule style, inline style)
// is serialized here so a shorthand reads back the same through both.
#include "domain/styling/css_declarations.h"
#include "domain/styling/css_box_value.h"
#include "domain/styling/css_font_value.h"
#include <algorithm>
#include <set>

namespace ruyijs::domain::styling {
namespace {
// Preferred order: the shorthand with the most longhands first.
constexpr const char* kShorthands[] = {
  "font", "border", "border-top", "border-right", "border-bottom", "border-left",
  "border-width", "border-style", "border-color", "margin", "padding", "inset",
  "flex", "flex-flow", "gap", "overflow"};
std::vector<std::string> Longhands(std::string_view shorthand) {
  if (shorthand != "font") return CssBoxLonghands(shorthand);
  std::vector<std::string> names;
  for (const auto& row : CssFontDefaults()) names.push_back(row.name);
  return names;
}
std::string Shorthand(const std::vector<CssDeclarationValue>& rows, std::string_view name, bool* important) {
  return name == "font" ? CssFontSerialize(rows, important) : CssBoxSerialize(rows, std::string(name), important);
}
}  // namespace

bool CssShorthandName(std::string_view name) { return name == "font" || CssBoxShorthand(name); }

std::string CssLookupDeclaration(const std::vector<CssDeclarationValue>& rows, const std::string& inputName,
                                 bool* important) {
  const auto name=CssCanonicalDeclarationName(inputName);
  *important = false;
  if (CssShorthandName(name)) return Shorthand(rows, name, important);
  for (const auto& row : rows)
    if (row.name == name) { *important = row.important; return row.value; }
  return {};
}

std::string CssSerializeBlock(const std::vector<CssDeclarationValue>& rows) {
  std::set<std::string> done;
  std::string out;
  for (const auto& row : rows) {
    if (done.count(row.name)) continue;
    std::string name = row.name, value = row.value;
    bool important = row.important;
    for (const char* shorthand : kShorthands) {
      const auto names = Longhands(shorthand);
      if (std::find(names.begin(), names.end(), row.name) == names.end()) continue;
      if (std::any_of(names.begin(), names.end(), [&](const auto& n) { return done.count(n) != 0; })) continue;
      bool flag = false;
      auto text = Shorthand(rows, shorthand, &flag);
      if (text.empty()) continue;
      name = shorthand; value = std::move(text); important = flag;
      done.insert(names.begin(), names.end());
      break;
    }
    done.insert(row.name);
    if (!out.empty()) out += ' ';
    out += name + ": " + value + (important ? " !important" : "") + ";";
  }
  return out;
}
}  // namespace ruyijs::domain::styling
