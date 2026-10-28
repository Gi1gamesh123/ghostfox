// CSS family names use the existing identifier/string scanner, including escapes.
#include "domain/styling/css_font_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_selector_tokens.h"
namespace ruyijs::domain::styling {
std::string CssFontFamily(std::string_view input) {
  std::string result;
  for (const auto& family : CssSplit(input, ',')) {
    if (family.empty()) return {};
    std::string name;
    size_t p = 0;
    if (family[0] == '\'' || family[0] == '"') {
      std::string decoded;
      if (!SelectorString(family, &p, &decoded) || CssSkip(family, p) != family.size()) return {};
      name = "\"";
      for (char c : decoded) { if (c == '"' || c == '\\') name += '\\'; name += c; }
      name += '"';
    } else {
      while ((p = CssSkip(family, p)) < family.size()) {
        std::string word;
        if (!SelectorIdent(family, &p, &word)) return {};
        if (!name.empty()) name += ' ';
        name += word;
        if (p < family.size() && !CssSpace(family[p]) && family.substr(p, 2) != "/*") return {};
      }
      const auto lower = CssLower(name);
      if (lower == "inherit" || lower == "initial" || lower == "unset" ||
          lower == "revert" || lower == "revert-layer" || lower == "default") return {};
      for (const auto& generic : CssSplit("serif,sans-serif,monospace,cursive,fantasy,system-ui,math,ui-serif,ui-sans-serif,ui-monospace,ui-rounded", ','))
        if (lower == generic) { name = lower; break; }
    }
    if (!result.empty()) result += ", ";
    result += name;
  }
  return result;
}
}  // namespace ruyijs::domain::styling
