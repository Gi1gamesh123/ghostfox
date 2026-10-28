#include "domain/styling/css_components.h"
namespace ruyijs::domain::styling {
bool CssSpace(char c) { return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f'; }
std::string CssTrim(std::string_view v) {
  while (!v.empty()&&CssSpace(v.front())) v.remove_prefix(1);
  while (!v.empty()&&CssSpace(v.back())) v.remove_suffix(1);
  return std::string(v);
}
std::string CssLower(std::string v) {
  for(char& c:v) if(c>='A'&&c<='Z') c+= 'a'-'A';
  return v;
}
size_t CssSkip(std::string_view v,size_t p) {
  while(p<v.size()) {
    if(CssSpace(v[p])) { ++p;continue; }
    if(v.substr(p,2)!="/*") break;
    auto q=v.find("*/",p+2);p=q==v.npos?v.size():q+2;
  }
  return p;
}
namespace {
size_t StringEnd(std::string_view v,size_t p) {
  const char quote=v[p++];
  while(p<v.size()) {
    if(v[p]=='\\') { p+=p+1<v.size()?2:1;continue; }
    if(v[p++]==quote) return p;
  }
  return v.size();
}
}
size_t CssDelimiter(std::string_view v,size_t p,std::string_view delimiters) {
  std::vector<char> closing;
  while(p<v.size()) {
    char c=v[p];
    if(c=='\''||c=='"') { p=StringEnd(v,p);continue; }
    if(v.substr(p,2)=="/*") { auto q=v.find("*/",p+2);p=q==v.npos?v.size():q+2;continue; }
    if(c=='\\') { p+=p+1<v.size()?2:1;continue; }
    if(closing.empty()&&delimiters.find(c)!=delimiters.npos) return p;
    if(c=='('||c=='['||c=='{') closing.push_back(c=='('?')':c=='['?']':'}');
    else if(!closing.empty()&&c==closing.back()) closing.pop_back();
    ++p;
  }
  return v.size();
}
size_t CssClosing(std::string_view v,size_t p) {
  if(p>=v.size()) return v.size();
  char last=v[p]=='{'?'}':v[p]=='('?')':']';
  return CssDelimiter(v,p+1,std::string(1,last));
}
std::string CssClean(std::string_view v) {
  std::string result;
  for(size_t p=0;p<v.size();) {
    if(v[p]=='\''||v[p]=='"') {
      auto end=StringEnd(v,p);result.append(v.substr(p,end-p));p=end;continue;
    }
    if(v.substr(p,2)=="/*") {
      auto end=v.find("*/",p+2);p=end==v.npos?v.size():end+2;continue;
    }
    result+=v[p++];
  }
  return CssTrim(result);
}
std::vector<std::string> CssSplit(std::string_view v,char separator) {
  std::vector<std::string> out;
  for(size_t p=0;p<=v.size();) {
    auto q=CssDelimiter(v,p,std::string(1,separator));out.push_back(CssTrim(v.substr(p,q-p)));
    if(q==v.size()) break;
    p=q+1;
  }
  return out;
}
std::string CssValueAtEof(std::string_view v) {
  std::string result(v), closing;
  for(size_t p=0;p<v.size();++p) {
    char c=v[p];
    if(c=='\\') { if(p+1<v.size()) ++p;continue; }
    if(c=='\''||c=='"') {
      const auto end=StringEnd(v,p);
      if(end==v.size()&&(end==p+1||v.back()!=c)) result+=c;
      p=end-1;continue;
    }
    if(v.substr(p,2)=="/*") {
      const auto end=v.find("*/",p+2);
      if(end==v.npos) {result+="*/";break;}
      p=end+1;continue;
    }
    if(c=='('||c=='['||c=='{') closing.push_back(c=='('?')':c=='['?']':'}');
    else if(!closing.empty()&&closing.back()==c) closing.pop_back();
  }
  for(auto i=closing.rbegin();i!=closing.rend();++i) result+=*i;
  return result;
}
bool CssSelectorValid(std::string_view v) {
  if(CssTrim(v).empty()) return false;
  std::vector<char> closing;
  for(size_t p=0;p<v.size();++p) {
    char c=v[p];
    if(c=='\\') { if(++p==v.size()) return false;continue; }
    if(c=='\''||c=='"') { p=StringEnd(v,p)-1;continue; }
    if(c=='?'||c=='{'||c=='}'||c==';') return false;
    if(c=='('||c=='[') closing.push_back(c=='('?')':']');
    if(c==')'||c==']') { if(closing.empty()||closing.back()!=c) return false;closing.pop_back(); }
  }
  for(const auto& s:CssSplit(v,',')) if(s.empty()) return false;
  return closing.empty();
}
std::string CssSelectorText(std::string_view v) {
  auto parts=CssSplit(CssClean(v),',');std::string result;
  for(auto& p:parts) { if(!result.empty()) result+=", ";result+=p; }
  return result;
}
}
