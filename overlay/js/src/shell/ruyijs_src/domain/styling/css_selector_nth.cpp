#include "domain/styling/css_selector_tokens.h"
#include "domain/styling/css_components.h"
#include <charconv>
namespace ruyijs::domain::styling {
namespace {
bool Integer(std::string_view text,int64_t* out) {
  if(text.empty()) return false;
  if(text[0]=='+') text.remove_prefix(1);
  if(text.empty()) return false;
  auto parsed=std::from_chars(text.data(),text.data()+text.size(),*out);
  return parsed.ec==std::errc()&&parsed.ptr==text.data()+text.size()&&*out>=-2147483648LL&&*out<=2147483647LL;
}
}
bool SelectorNth(std::string_view text,int64_t* a,int64_t* b,std::string* filter) {
  auto input=CssClean(text),lower=CssLower(input);size_t pos=std::string::npos;
  for(size_t i=1;i+2<lower.size();++i)
    if(lower.substr(i,2)=="of"&&CssSpace(lower[i-1])&&CssSpace(lower[i+2])) {pos=i;break;}
  if(pos!=std::string::npos) {*filter=CssTrim(std::string_view(input).substr(pos+2));lower=CssTrim(std::string_view(lower).substr(0,pos));if(filter->empty()) return false;}
  if(lower=="odd") {*a=2;*b=1;return true;}
  if(lower=="even") {*a=2;*b=0;return true;}
  auto n=lower.find('n');if(n==lower.npos) {*a=0;return Integer(lower,b);}
  auto factor=lower.substr(0,n),offset=CssTrim(std::string_view(lower).substr(n+1));
  if(factor.empty()||factor=="+") *a=1;
  else if(factor=="-") *a=-1;
  else if(!Integer(factor,a)) return false;
  if(offset.empty()) {*b=0;return true;}
  if(offset[0]!='+'&&offset[0]!='-') return false;
  const char sign=offset[0];auto number=CssTrim(std::string_view(offset).substr(1));
  if(number.empty()||number[0]=='+'||number[0]=='-') return false;
  return Integer(std::string(1,sign)+number,b);
}
}
