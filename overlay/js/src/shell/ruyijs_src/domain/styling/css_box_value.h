// Box declarations are interpreted by the original Styling parser and cascade.
#pragma once
#include "domain/styling/css_declarations.h"
#include <string_view>
namespace ruyijs::domain::styling {
class StyleDeclaration;
bool CssBoxShorthand(std::string_view);
std::vector<std::string> CssBoxLonghands(std::string_view);
bool CssBoxExpand(const std::string&, const std::string&, bool, std::vector<CssDeclarationValue>*);
std::string CssBoxSerialize(const std::vector<CssDeclarationValue>&, const std::string&, bool* = nullptr);
void CssComputeBox(StyleDeclaration&, double root_size);
}
