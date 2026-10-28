#include "domain/styling/css_page_value.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_selector_tokens.h"
#include "domain/styling/css_identifier.h"
namespace ruyijs::domain::styling {
namespace {
size_t PageComments(std::string_view text,size_t p) {
  while(text.substr(p,2)=="/*") {
    const auto end=text.find("*/",p+2);p=end==text.npos?text.size():end+2;
  }
  return p;
}
}
bool CssPageSelector(std::string_view text,std::string* out) {
  size_t p=CssSkip(text,0);std::string result;
  if(p==text.size()) {out->clear();return true;}
  while(p<text.size()) {
    std::string selector,name;
    if(text[p]!=':') {
      if(!SelectorIdent(text,&p,&name)) return false;
      selector=CssEscapeIdentifier(name);
    }
    p=PageComments(text,p);
    while(p<text.size()&&text[p]==':') {
      p=PageComments(text,p+1);name.clear();
      if(!SelectorIdent(text,&p,&name)) return false;
      name=CssLower(name);
      if(name!="left"&&name!="right"&&name!="first"&&name!="blank") return false;
      selector+=':'+name;p=PageComments(text,p);
    }
    if(selector.empty()) return false;
    if(!result.empty()) result+=", ";result+=selector;
    p=CssSkip(text,p);
    if(p==text.size()) break;
    if(text[p++]!=',') return false;
    p=CssSkip(text,p);if(p==text.size()) return false;
  }
  *out=std::move(result);return true;
}
bool CssPagePrelude(std::string_view text,std::string* out) {
  size_t p=0;std::string keyword;
  return !text.empty()&&text[p++]=='@'&&SelectorIdent(text,&p,&keyword)&&
    CssLower(keyword)=="page"&&CssPageSelector(text.substr(p),out);
}
std::vector<CssDeclarationValue> CssPageBody(std::string_view text) {
  std::string declarations;
  for(size_t p=0;(p=CssSkip(text,p))<text.size();) {
    const auto end=CssDelimiter(text,p,";{");
    if(end==text.size()||text[end]==';') {
      declarations+=std::string(text.substr(p,end-p))+";";
      p=end==text.size()?end:end+1;continue;
    }
    const auto close=CssClosing(text,end);
    // Custom properties may contain blocks; unknown page margin rules do not
    // consume a following descriptor after their closing brace.
    if(text.substr(p,2)=="--"&&CssDelimiter(text,p,":")<end) {
      const auto next=CssDelimiter(text,close==text.size()?close:close+1,";");
      declarations+=std::string(text.substr(p,next-p))+";";
      p=next==text.size()?next:next+1;
    } else p=close==text.size()?close:close+1;
  }
  return CssDeclarations(declarations,CssDeclarationContext::Page);
}
}
