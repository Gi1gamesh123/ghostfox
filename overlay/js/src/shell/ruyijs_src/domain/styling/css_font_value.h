// Font shorthand values belong to the original Styling declaration/cascade.
#pragma once
#include "domain/styling/css_declarations.h"
#include <string_view>
namespace ruyijs::domain::styling {
class StyleDeclaration;
const std::vector<CssDeclarationValue>& CssFontDefaults();
// The initial font-family: the platform's default generic for the locale's
// language group (Firefox font.default.<group>). Set once before any cascade.
void CssSetInitialFontFamily(std::string generic);
const std::string& CssInitialFontFamily();
bool CssFontExpand(const std::string&, bool, std::vector<CssDeclarationValue>*);
// Whether `value` is one of the CSS system-font keywords the font shorthand
// accepts in content. Firefox 158 accepts the six CSS2 keywords; its own
// -moz-window/-moz-dialog/... aliases are not exposed to content, so a value
// like `-apple-system-body` or `-moz-window` is not a font shorthand value.
bool CssSystemFontKeyword(const std::string&);
std::string CssFontSerialize(const std::vector<CssDeclarationValue>&, bool* = nullptr);
std::string CssFontFamily(std::string_view);
void CssComputeFont(StyleDeclaration&, const StyleDeclaration&, double);
std::string CssResolvedLineHeight(const StyleDeclaration&);
std::string CssFontNumber(double);
}  // namespace ruyijs::domain::styling
