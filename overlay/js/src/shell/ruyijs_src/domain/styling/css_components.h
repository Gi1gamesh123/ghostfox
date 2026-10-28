// Engine-independent CSS component scanner shared by rule and condition parsing.
#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace ruyijs::domain::styling {
bool CssSpace(char);
std::string CssTrim(std::string_view);
std::string CssLower(std::string);
size_t CssSkip(std::string_view,size_t);
size_t CssDelimiter(std::string_view,size_t,std::string_view);
size_t CssClosing(std::string_view,size_t);
std::string CssClean(std::string_view);
std::string CssValueAtEof(std::string_view);
std::vector<std::string> CssSplit(std::string_view,char);
bool CssSelectorValid(std::string_view);
std::string CssSelectorText(std::string_view);
}
