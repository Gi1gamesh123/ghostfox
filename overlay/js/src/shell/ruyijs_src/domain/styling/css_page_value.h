#pragma once
#include "domain/styling/css_declarations.h"
#include <string_view>
namespace ruyijs::domain::styling {
bool CssPageSelector(std::string_view,std::string*);
bool CssPagePrelude(std::string_view,std::string*);
std::vector<CssDeclarationValue> CssPageBody(std::string_view);
bool CssPageProperty(std::string_view);
std::vector<std::string> CssPageLonghands(std::string_view);
bool CssPageExpand(const std::string&,const std::string&,bool,std::vector<CssDeclarationValue>*);
std::string CssPageLength(const std::string&);
bool CssPageWide(std::string_view);
std::string CssPageSerialize(const std::vector<CssDeclarationValue>&,const std::string&,bool* = nullptr);
std::string CssPageText(const std::vector<CssDeclarationValue>&);
std::string CssPageLookup(const std::vector<CssDeclarationValue>&,const std::string&,bool*);
std::string CssPageSet(std::vector<CssDeclarationValue>&,const std::string&,const std::string&,bool,bool);
}
