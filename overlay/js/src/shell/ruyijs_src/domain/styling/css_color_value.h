#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace ruyijs::domain::styling {
bool CssColorSupported(std::string_view,unsigned depth=0);
// css_color_names.cpp: whether a name is a colour name, and the sRGB value it
// stands for (false for the names whose value is not the table's, e.g. the
// transparent keyword is handled here too).
bool CssColorNameKnown(std::string_view);
bool CssColorNameRgba(std::string_view,uint8_t rgba[4]);
// css_color_resolve.cpp: the 8-bit sRGB value of a CSS colour value.
bool CssColorRgba(std::string_view,uint8_t rgba[4]);
// css_color_computed.cpp: the computed (serialized) form of a colour value,
// rgb(r, g, b) / rgba(r, g, b, a); currentcolor resolves to `current_color`.
// Empty when the value is not a colour (e.g. auto).
std::string CssComputedColor(std::string_view value, std::string_view current_color);
bool CssSelectorSupported(std::string_view,unsigned depth=0);
bool CssValueComponentsValid(std::string_view,bool custom=false);
bool CssVariableReferenceValid(std::string_view);
}
