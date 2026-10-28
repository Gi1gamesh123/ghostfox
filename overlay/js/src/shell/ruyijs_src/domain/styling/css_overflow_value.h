// The overflow shorthand and its computed-value rule (css_overflow_value.cpp).
#pragma once
#include "domain/styling/css_declarations.h"
#include <string_view>
namespace ruyijs::domain::styling {
class StyleDeclaration;
bool CssOverflowShorthand(std::string_view);
bool CssOverflowLonghand(std::string_view);
bool CssOverflowKeyword(const std::string&);
bool CssOverflowExpand(const std::string&, bool important, std::vector<CssDeclarationValue>*);
std::string CssOverflowSerialize(const std::vector<CssDeclarationValue>&, bool* important = nullptr);
void CssComputeOverflow(StyleDeclaration&);
}  // namespace ruyijs::domain::styling
