#include "domain/styling/css_condition_value.h"
#include "domain/styling/css_components.h"
namespace ruyijs::domain::styling {
namespace {
bool ConditionOperand(const std::string& text) {
  auto open=text.find('(');
  if(open==text.npos||CssClosing(text,open)!=text.size()-1) return false;
  for(size_t p=0;p<open;++p) {
    unsigned char c=text[p];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='-'||c=='_'||c>=128)) return false;
  }
  return true;
}
}
CssConditionResult CssBooleanCondition(std::string_view value,const CssConditionAtom& atom,unsigned depth) {
  if(depth>64) return {};
  std::string text=CssClean(value);
  if(text.empty()) return {};
  std::vector<std::string> parts;std::string operation;size_t start=0;
  for(size_t p=0;p<text.size();) {
    char c=text[p];
    if(c=='('||c=='['||c=='{') { auto end=CssClosing(text,p);if(end==text.size()) return {};p=end+1;continue; }
    if(c=='\''||c=='"') { ++p;while(p<text.size()&&text[p]!=c) {if(text[p]=='\\') ++p;++p;}++p;continue; }
    if(CssSpace(c)) {
      auto q=CssSkip(text,p);auto end=q;
      while(end<text.size()&&!CssSpace(text[end])) ++end;
      auto word=CssLower(text.substr(q,end-q));
      if(word=="and"||word=="or") {
        if(!operation.empty()&&operation!=word) return {};
        operation=word;parts.push_back(CssTrim(std::string_view(text).substr(start,p-start)));
        start=CssSkip(text,end);p=start;
        if(start==text.size()) return {};
        continue;
      }
    }
    ++p;
  }
  if(!operation.empty()) {
    if(CssLower(text).substr(0,4)=="not ") return {};
    parts.push_back(CssTrim(std::string_view(text).substr(start)));
    if(parts.size()<2) return {};
    CssConditionResult result{true,operation=="and",true};
    for(const auto& p:parts) {
      if(!ConditionOperand(p)) return {};
      auto item=CssBooleanCondition(p,atom,depth+1);if(!item.valid) return {};
      bool decisive=operation=="and"?((result.known&&!result.value)||(item.known&&!item.value)):
                                      ((result.known&&result.value)||(item.known&&item.value));
      result.known=decisive||(result.known&&item.known);
      result.value=operation=="and"?result.value&&item.value:result.value||item.value;
    }
    return result;
  }
  if(CssLower(text).substr(0,4)=="not ") {
    auto tail=CssTrim(std::string_view(text).substr(4));
    if(!ConditionOperand(tail)) return {};
    auto item=CssBooleanCondition(tail,atom,depth+1);
    return {item.valid,item.valid&&!item.value,item.known};
  }
  if(text.front()=='('&&CssClosing(text,0)==text.size()-1) {
    auto inside=std::string_view(text).substr(1,text.size()-2);
    return CssBooleanCondition(inside,atom,depth+1);
  }
  if(text.front()=='(') return {};
  return atom(text);
}
}
