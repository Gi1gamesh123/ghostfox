// Shared Gecko-compatible shortest-f32 decimal serialization.
#include "domain/styling/css_float_text.h"
#include <charconv>
namespace ruyijs::domain::styling {
std::string CssFloatText(float number) {
  if(number==0) return "0";
  const bool negative=number<0;if(negative) number=-number;
  // Gecko's dtoa-short rounds the shortest f32 decimal to six significant
  // digits. Rounding a widened double instead changes decimal ties and the
  // fixed/scientific boundary at 1e-6. Keep notation's exponent before carry.
  char buf[64];auto encoded=std::to_chars(buf,buf+sizeof(buf),number,std::chars_format::scientific);
  if(encoded.ec!=std::errc{}) return {};
  std::string digits;const char* cursor=buf;
  while(cursor<encoded.ptr&&*cursor!='e') {if(*cursor!='.') digits+=*cursor;++cursor;}
  if(cursor==encoded.ptr) return {};
  ++cursor;if(cursor<encoded.ptr&&*cursor=='+') ++cursor;
  int exponent=0;
  if(std::from_chars(cursor,encoded.ptr,exponent).ec!=std::errc{}) return {};
  const bool scientific=exponent<-6;
  int point=scientific?1:exponent+1;
  if(digits.size()>6) {
    const bool round=digits[6]>='5';digits.resize(6);
    if(round) {
      size_t end=digits.size();
      while(end&&digits[end-1]=='9') digits[--end]='0';
      if(end) ++digits[end-1];
      else {digits.insert(digits.begin(),'1');++point;}
    }
  }
  while(digits.size()>1&&digits.back()=='0') digits.pop_back();
  std::string result;
  if(point<=0) result="0."+std::string(size_t(-point),'0')+digits;
  else if(size_t(point)>=digits.size()) result=digits+std::string(size_t(point)-digits.size(),'0');
  else result=digits.substr(0,point)+"."+digits.substr(point);
  if(scientific) result+='e'+std::to_string(exponent);
  return (negative?"-":"")+result;
}
}
