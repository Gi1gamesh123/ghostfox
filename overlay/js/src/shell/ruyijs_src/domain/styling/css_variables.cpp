// var() substitution and custom property resolution (css_variables.h).
#include "domain/styling/css_variables.h"
#include "domain/styling/stylesheet.h"
#include <optional>
#include <vector>

namespace ruyijs::domain::styling {
namespace {
constexpr unsigned kMaxDepth = 32;
constexpr size_t kMaxLength = 1 << 20;  // Bounds a substitution that keeps growing.
bool NameChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
         c == '-' || c == '_' || static_cast<unsigned char>(c) >= 0x80;
}
std::string Trim(const std::string& text) {
  const auto begin = text.find_first_not_of(" \t\r\n\f");
  if (begin == text.npos) return {};
  return text.substr(begin, text.find_last_not_of(" \t\r\n\f") - begin + 1);
}
// The index of the bracket closing the one at |open| (npos when unbalanced),
// or with |comma| the first top-level ',' from |open| (npos when none).
size_t Scan(const std::string& text, size_t open, bool comma) {
  int depth = comma ? 1 : 0;
  for (size_t i = open; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '\\') { ++i; continue; }
    if (c == '"' || c == '\'') {
      for (++i; i < text.size() && text[i] != c; ++i) if (text[i] == '\\') ++i;
      continue;
    }
    if (c == '(' || c == '[' || c == '{') ++depth;
    else if (c == ')' || c == ']' || c == '}') { if (--depth == 0 && !comma) return i; }
    else if (comma && c == ',' && depth == 1) return i;
  }
  return std::string::npos;
}
size_t FindVar(const std::string& text, size_t from) {
  for (size_t at = from; at + 4 <= text.size(); ++at) {
    if ((text[at] | 0x20) == 'v' && (text[at + 1] | 0x20) == 'a' && (text[at + 2] | 0x20) == 'r' &&
        text[at + 3] == '(' && (at == 0 || !NameChar(text[at - 1])))
      return at;
  }
  return std::string::npos;
}
bool Substitute(const std::string& value, const std::function<bool(const std::string&, std::string*)>& lookup,
                std::string* out, unsigned depth) {
  if (depth > kMaxDepth) return false;
  std::string result;
  for (size_t p = 0;;) {
    const size_t at = FindVar(value, p);
    if (at == std::string::npos) { result.append(value, p, std::string::npos); break; }
    result.append(value, p, at - p);
    const size_t close = Scan(value, at + 3, false);
    if (close == std::string::npos) return false;
    const auto inner = value.substr(at + 4, close - at - 4);
    const size_t comma = Scan(inner, 0, true);
    const auto name = Trim(inner.substr(0, comma));
    if (name.size() < 3 || name.compare(0, 2, "--") != 0) return false;
    std::string substitution;
    if (!lookup(name, &substitution)) {
      if (comma == std::string::npos) return false;
      if (!Substitute(inner.substr(comma + 1), lookup, &substitution, depth + 1)) return false;
    }
    result += Trim(substitution);
    if (result.size() > kMaxLength) return false;
    p = close + 1;
  }
  *out = Trim(result);
  return true;
}
bool Custom(const std::string& name) { return name.size() > 2 && name.compare(0, 2, "--") == 0; }
}  // namespace

bool CssSubstituteVariables(const std::string& value,
                            const std::function<bool(const std::string&, std::string*)>& lookup,
                            std::string* out) {
  return Substitute(value, lookup, out, 0);
}

void CssResolveVariables(StyleDeclaration& result, const std::map<std::string, uint64_t>& order,
                         const CssVariableRules& rules) {
  const auto unset = [&](const std::string& name) {
    if (rules.inherited(name) && rules.parent && rules.parent->has_property(name))
      result.set_property(name, rules.parent->get_property_value(name), false);
    else if (Custom(name)) result.remove_property(name);
    else result.set_property(name, rules.initial(name), false);
  };
  // Custom properties first: each resolves against the element's own (or
  // inherited, already resolved) custom properties; a cycle is invalid.
  std::map<std::string, std::optional<std::string>> resolved;
  std::map<std::string, bool> visiting;
  std::function<bool(const std::string&, std::string*)> custom = [&](const std::string& name, std::string* out) {
    const auto done = resolved.find(name);
    if (done != resolved.end()) {
      if (!done->second) return false;
      *out = *done->second;
      return true;
    }
    if (!result.has_property(name) || visiting[name]) return false;
    visiting[name] = true;
    const auto raw = result.get_property_value(name);
    std::string value = raw;
    const bool ok = FindVar(raw, 0) == std::string::npos || Substitute(raw, custom, &value, 0);
    visiting[name] = false;
    resolved[name] = ok ? std::optional<std::string>(value) : std::nullopt;
    if (ok) *out = value;
    return ok;
  };
  // Inherited values were already resolved on the parent; only this element's
  // own declarations can still hold var() or redefine a custom property.
  std::vector<std::string> names;
  for (const auto& entry : order) if (result.has_property(entry.first)) names.push_back(entry.first);
  for (const auto& name : names) {
    if (!Custom(name)) continue;
    std::string value;
    if (custom(name, &value)) {
      if (value != result.get_property_value(name))
        result.set_property(name, value, result.get_property_priority(name));
    } else {
      unset(name);
    }
  }
  const auto final_lookup = [&](const std::string& name, std::string* out) {
    if (!result.has_property(name)) return false;
    *out = result.get_property_value(name);
    return true;
  };
  for (const auto& name : names) {
    if (Custom(name) || !result.has_property(name)) continue;
    const auto raw = result.get_property_value(name);
    if (FindVar(raw, 0) == std::string::npos) continue;
    const bool important = result.get_property_priority(name);
    std::string value;
    const bool ok = Substitute(raw, final_lookup, &value, 0);
    const bool shorthand = CssBoxShorthand(name) || name == "font";
    if (!shorthand) {
      if (ok) result.set_property(name, value, important);
      else unset(name);
      continue;
    }
    // A pending shorthand expands now; a longhand declared after it wins.
    result.remove_property(name);
    const auto declared = order.find(name);
    const uint64_t at = declared == order.end() ? 0 : declared->second;
    const auto later = [&](const std::string& longhand) {
      const auto it = order.find(longhand);
      return it != order.end() && it->second > at;
    };
    std::vector<CssDeclarationValue> rows;
    if (ok) rows = CssDeclarations(name + ":" + value);
    std::vector<std::string> longhands;
    if (name == "font") for (const auto& row : CssFontDefaults()) longhands.push_back(row.name);
    else longhands = CssBoxLonghands(name);
    for (const auto& longhand : longhands) {
      if (later(longhand)) continue;
      const auto row = std::find_if(rows.begin(), rows.end(), [&](const auto& r) { return r.name == longhand; });
      if (row != rows.end()) result.set_property(longhand, row->value, important);
      else if (rows.empty()) unset(longhand);
    }
  }
}
}  // namespace ruyijs::domain::styling
