#include "domain/styling/css_selector.h"
#include "domain/styling/css_selector_tokens.h"
#include "domain/styling/css_components.h"
namespace ruyijs::domain::styling {
namespace {
std::string SelectorSpacing(const std::string& text) {
  std::string out;
  for(size_t p=0;p<text.size();) {
    if(text[p]=='\''||text[p]=='"') {
      size_t start=p;std::string value;
      if(!SelectorString(text,&p,&value)) return text;
      out+=text.substr(start,p-start);continue;
    }
    if(text[p]=='\\') {size_t start=p;std::string value;if(!SelectorIdent(text,&p,&value)) return text;
      out+=text.substr(start,p-start);continue;}
    if(text[p]==',') {while(!out.empty()&&CssSpace(out.back())) out.pop_back();out+=", ";p=CssSkip(text,p+1);continue;}
    out+=text[p++];
  }
  return out;
}
bool Anchor(const std::string& text) {
  for(size_t p=0;p<text.size();) {
    if(text[p]=='[') {p=CssClosing(text,p);if(p<text.size()) ++p;continue;}
    if(text[p]=='\\') {std::string value;if(!SelectorIdent(text,&p,&value)) return false;continue;}
    if(text[p++]=='&') return true;
  }
  return false;
}
}
std::string CssNestedSelectorText(const std::string& input) {
  std::string result;
  for(auto part:CssSplit(CssClean(input),',')) {
    if(!result.empty()) result+=", ";
    if(!Anchor(part)) result+="& ";
    result+=SelectorSpacing(part);
  }
  return result;
}
}
