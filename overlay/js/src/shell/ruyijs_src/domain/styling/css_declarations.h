#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace ruyijs::domain::styling {
struct CssDeclarationValue { std::string name,value;bool important=false;std::string pending_shorthand; };
enum class CssDeclarationContext { Standard, Page };
// Native Firefox transform aliases share one declaration value and priority.
std::string CssCanonicalDeclarationName(const std::string&);
std::vector<CssDeclarationValue> CssDeclarations(const std::string&,
    CssDeclarationContext = CssDeclarationContext::Standard);
std::string CssDeclarationText(const std::string&);
std::string CssSetDeclaration(std::string&,const std::string&,const std::string&,bool,bool);
// css_block_serialization.cpp: CSSOM block serialization (shorthands recombined)
// and the shorthand-aware value lookup shared by every declaration block.
bool CssShorthandName(std::string_view);
std::string CssSerializeBlock(const std::vector<CssDeclarationValue>&);
std::string CssLookupDeclaration(const std::vector<CssDeclarationValue>&,const std::string&,bool* important);
}
