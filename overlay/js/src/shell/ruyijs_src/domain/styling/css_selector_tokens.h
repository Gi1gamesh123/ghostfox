#pragma once
#include "domain/styling/css_selector.h"
#include <string_view>
namespace ruyijs::domain::styling {
bool SelectorIdent(std::string_view,size_t*,std::string*);
bool SelectorString(std::string_view,size_t*,std::string*);
bool SelectorAttributeParse(std::string_view,SelectorAttribute*);
bool SelectorAttributeMatches(const ElementNode&,const SelectorAttribute&);
bool SelectorNth(std::string_view,int64_t*,int64_t*,std::string*);
bool SelectorPseudoKnown(const std::string&,bool element=false);
bool SelectorPseudoMatches(const ElementNode&,const std::string&,const ElementNode*);
}
