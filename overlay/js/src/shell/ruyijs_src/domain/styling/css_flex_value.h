// The flex, flex-flow and gap shorthands; dispatched from css_box_value.cpp.
#pragma once
#include "domain/styling/css_declarations.h"
#include <string_view>
namespace ruyijs::domain::styling {
bool CssFlexShorthand(std::string_view);
std::vector<std::string> CssFlexLonghands(std::string_view);
bool CssFlexExpand(const std::string&, const std::string&, bool, std::vector<CssDeclarationValue>*);
std::string CssFlexSerialize(const std::vector<CssDeclarationValue>&, const std::string&, bool* = nullptr);
}
