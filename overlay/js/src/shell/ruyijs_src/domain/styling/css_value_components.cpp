#include "domain/styling/css_color_value.h"
#include "domain/styling/css_components.h"
namespace ruyijs::domain::styling {
bool CssValueComponentsValid(std::string_view value,bool custom) {
  std::vector<char> stack;bool quote=false;char q=0;
  for(size_t p=0;p<value.size();++p) {
    char c=value[p];
    if(c=='\\') {if(p+1==value.size()) return false;++p;continue;}
    if(quote) {if(c==q) quote=false;else if(c=='\n'||c=='\r'||c=='\f') return false;continue;}
    if(c=='\''||c=='"') {quote=true;q=c;continue;}
    if(value.substr(p,2)=="/*") {auto end=value.find("*/",p+2);if(end==value.npos) break;p=end+1;continue;}
    if(stack.empty()&&(c==';'||c=='!')) return false;
    if(!custom&&(c=='{'||c=='}')) return false;
    if(c=='('||c=='['||c=='{') stack.push_back(c=='('?')':c=='['?']':'}');
    else if(c==')'||c==']'||c=='}') {if(stack.empty()||stack.back()!=c) return false;stack.pop_back();}
  }
  return !quote&&stack.empty();
}
bool CssVariableReferenceValid(std::string_view text) {
  bool seen=false;
  for(size_t p=0;p+4<=text.size();++p) {
    if(text.substr(p,4)!="var("&&text.substr(p,4)!="env(") continue;
    if(p&&((text[p-1]>='a'&&text[p-1]<='z')||text[p-1]=='-')) continue;
    auto close=CssClosing(text,p+3);if(close==text.size()) return false;
    auto body=text.substr(p+4,close-p-4);auto comma=CssDelimiter(body,0,",");
    auto name=CssTrim(body.substr(0,comma));bool variable=text.substr(p,4)=="var(";
    if(name.empty()||(variable&&(name.size()<=2||name.substr(0,2)!="--"))) return false;
    for(char c:name) if(CssSpace(c)||c==';'||c=='!'||c=='('||c==')') return false;
    seen=true;
  }
  return seen;
}
}
