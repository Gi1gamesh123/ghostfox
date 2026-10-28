#include "domain/styling/css_selector_tokens.h"
#include "domain/styling/css_components.h"
namespace ruyijs::domain::styling {
namespace {
bool Name(unsigned char c) {return c>=128||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||c=='-';}
bool SelectorDigit(char c) {return c>='0'&&c<='9';}
int Hex(char c) {
  if(SelectorDigit(c)) return c-'0';
  if(c>='a'&&c<='f') return c-'a'+10;
  if(c>='A'&&c<='F') return c-'A'+10;
  return -1;
}
void Utf8(uint32_t cp,std::string* out) {
  if(!cp||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)) cp=0xfffd;
  if(cp<128) out->push_back(char(cp));
  else if(cp<2048) {out->push_back(char(0xc0|(cp>>6)));out->push_back(char(0x80|(cp&63)));}
  else if(cp<65536) {out->push_back(char(0xe0|(cp>>12)));out->push_back(char(0x80|((cp>>6)&63)));out->push_back(char(0x80|(cp&63)));}
  else {out->push_back(char(0xf0|(cp>>18)));out->push_back(char(0x80|((cp>>12)&63)));
    out->push_back(char(0x80|((cp>>6)&63)));out->push_back(char(0x80|(cp&63)));}
}
bool Escape(std::string_view text,size_t* pos,std::string* out) {
  size_t& p=*pos;if(++p==text.size()||text[p]=='\n'||text[p]=='\r'||text[p]=='\f') return false;
  if(Hex(text[p])<0) {out->push_back(text[p++]);return true;}
  uint32_t cp=0;unsigned n=0;
  while(p<text.size()&&Hex(text[p])>=0&&n++<6) cp=cp*16+Hex(text[p++]);
  if(p<text.size()&&CssSpace(text[p])) {if(text[p++]=='\r'&&p<text.size()&&text[p]=='\n') ++p;}
  Utf8(cp,out);return true;
}
}
bool SelectorIdent(std::string_view text,size_t* pos,std::string* out) {
  size_t& p=*pos;if(p==text.size()) return false;
  if(!Name(static_cast<unsigned char>(text[p]))&&text[p]!='\\') return false;
  if(text[p]=='-'&&(p+1==text.size()||(!Name(static_cast<unsigned char>(text[p+1]))&&text[p+1]!='\\'))) return false;
  size_t start=p;
  while(p<text.size()) {
    if(text[p]=='\\') {if(!Escape(text,&p,out)) return false;}
    else if(Name(static_cast<unsigned char>(text[p]))||SelectorDigit(text[p])) out->push_back(text[p++]);
    else break;
  }
  return p>start;
}
bool SelectorString(std::string_view text,size_t* pos,std::string* out) {
  size_t& p=*pos;if(p==text.size()||(text[p]!='\''&&text[p]!='"')) return false;
  char quote=text[p++];
  while(p<text.size()) {
    if(text[p]==quote) {++p;return true;}
    if(text[p]=='\n'||text[p]=='\r'||text[p]=='\f') return false;
    if(text[p]=='\\') {if(!Escape(text,&p,out)) return false;}
    else out->push_back(text[p++]);
  }
  return false;
}
bool SelectorAttributeParse(std::string_view text,SelectorAttribute* out) {
  size_t p=CssSkip(text,0);if(!SelectorIdent(text,&p,&out->name)) return false;
  p=CssSkip(text,p);if(p==text.size()) return true;
  if(text[p]=='=') {out->op="=";++p;}
  else if(p+1<text.size()&&std::string("~|^$*").find(text[p])!=std::string::npos&&text[p+1]=='=') {
    out->op=std::string(text.substr(p,2));p+=2;
  } else return false;
  p=CssSkip(text,p);if(p==text.size()) return false;
  if(text[p]=='\''||text[p]=='"') {if(!SelectorString(text,&p,&out->value)) return false;}
  else if(!SelectorIdent(text,&p,&out->value)) return false;
  p=CssSkip(text,p);
  if(p<text.size()) {
    char c=text[p++];if(c>='A'&&c<='Z') c+='a'-'A';
    if(c!='i'&&c!='s') return false;
    out->flag=c;p=CssSkip(text,p);
  }
  return p==text.size();
}
bool SelectorAttributeMatches(const ElementNode& node,const SelectorAttribute& attr) {
  auto key=node.html?CssLower(attr.name):attr.name;auto it=node.attributes.find(key);
  if(it==node.attributes.end()) return false;
  if(attr.op.empty()) return true;
  auto value=it->second,needle=attr.value;
  const std::string insensitive="|type|dir|draggable|contenteditable|translate|";
  if(attr.flag=='i'||(!attr.flag&&node.html&&insensitive.find("|"+key+"|")!=std::string::npos)) {
    value=CssLower(value);needle=CssLower(needle);
  }
  if(attr.op=="=") return value==needle;
  if(attr.op=="|=") return value==needle||value.substr(0,needle.size()+1)==needle+"-";
  if(needle.empty()) return false;
  if(attr.op=="^=") return value.starts_with(needle);
  if(attr.op=="$=") return value.ends_with(needle);
  if(attr.op=="*=") return value.find(needle)!=std::string::npos;
  if(attr.op=="~=") {
    if(needle.find_first_of(" \t\n\r\f")!=std::string::npos) return false;
    for(size_t p=0;p<value.size();) {
      while(p<value.size()&&CssSpace(value[p])) ++p;
      size_t end=p;while(end<value.size()&&!CssSpace(value[end])) ++end;
      if(value.substr(p,end-p)==needle) return true;
      p=end;
    }
  }
  return false;
}
}
