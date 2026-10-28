// CSS Custom Properties for Cascading Variables 1 (§2, §3): computed values of
// custom properties and var() substitution, run by the cascade owner
// (stylesheet.h compute_style) before font and box computation.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
namespace ruyijs::domain::styling {
class StyleDeclaration;
// Substitutes every var() in |value|; returns false (and leaves *out alone)
// when a reference is to an invalid custom property without a fallback.
bool CssSubstituteVariables(const std::string& value,
                            const std::function<bool(const std::string&, std::string*)>& lookup,
                            std::string* out);
struct CssVariableRules {
  std::function<bool(const std::string&)> inherited;
  std::function<std::string(const std::string&)> initial;
  const StyleDeclaration* parent = nullptr;
};
// Resolves |result| in place. |order| is each declared property's cascade
// order, so a longhand declared after a var()-bearing shorthand keeps its own
// value. A property invalid at computed-value time behaves as unset.
void CssResolveVariables(StyleDeclaration& result, const std::map<std::string, uint64_t>& order,
                         const CssVariableRules& rules);
}  // namespace ruyijs::domain::styling
