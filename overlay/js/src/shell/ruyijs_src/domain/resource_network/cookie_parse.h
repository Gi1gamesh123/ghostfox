#pragma once
#include "domain/resource_network/cookie_store.h"
#include <cstdlib>

namespace ruyijs::domain::resource_network {
inline bool ParseCookie(const std::string& text,const CookieURL& url,double now,Cookie* out) {
  if((url.protocol()!="http:"&&url.protocol()!="https:")||text.size()>16384)return false;
  for(unsigned char c:text)if((c<32&&c!=9)||c==127)return false;
  auto semi=text.find(';');std::string pair=text.substr(0,semi);
  auto equal=pair.find('=');
  out->name=CookieTrim(pair.substr(0,equal));
  out->value=equal==std::string::npos?"":CookieTrim(pair.substr(equal+1));
  if(out->name.empty()||out->name.size()+out->value.size()>4096)return false;
  out->domain=url.hostname();
  auto slash=url.pathname().rfind('/');
  out->path=slash==std::string::npos||slash==0?"/":url.pathname().substr(0,slash);
  bool explicitDomain=false,explicitPath=false,ageSet=false;
  double age=0,expires=std::numeric_limits<double>::infinity();
  while(semi!=std::string::npos) {
    auto next=text.find(';',semi+1);
    std::string attribute=CookieTrim(text.substr(semi+1,next==std::string::npos?next:next-semi-1));
    auto eq=attribute.find('=');
    auto name=CookieLower(CookieTrim(attribute.substr(0,eq)));
    auto value=eq==std::string::npos?"":CookieTrim(attribute.substr(eq+1));
    if(name=="domain"&&!value.empty()) {
      value=CookieLower(value);if(value.front()=='.')value.erase(0,1);
      if(value.empty()||value.back()=='.'||!CookieDomain(url.hostname(),value))return false;
      if(CookieSuffix(value)==value) {if(value!=url.hostname())return false;}
      else out->hostOnly=false;
      explicitDomain=true;out->domain=value;
    } else if(name=="path"&&!value.empty()&&value.front()=='/') {out->path=value;explicitPath=true;}
    else if(name=="secure")out->secure=true;
    else if(name=="httponly")out->httpOnly=true;
    else if(name=="samesite") {
      value=CookieLower(value);
      out->sameSite=value=="none"?CookieSameSite::None:value=="strict"?CookieSameSite::Strict:
        value=="lax"?CookieSameSite::Lax:CookieSameSite::Default;
    } else if(name=="expires") {double date;if(CookieDate(value,&date))expires=date;}
    else if(name=="max-age"&&!value.empty()) {
      size_t start=value.front()=='-'?1:0;
      if(start<value.size()&&std::all_of(value.begin()+start,value.end(),[](char c){return c>='0'&&c<='9';})) {
        ageSet=true;age=0;
        for(size_t i=start;i<value.size();++i)age=std::min(400.0*86400,age*10+value[i]-'0');
        if(start)age=-age;
      }
    }
    semi=next;
  }
  if(out->secure&&!CookieSecure(url))return false;
  if(out->sameSite==CookieSameSite::None&&!out->secure)return false;
  if(out->name.compare(0,9,"__Secure-")==0&&!out->secure)return false;
  if(out->name.compare(0,7,"__Host-")==0&&(!out->secure||explicitDomain||!explicitPath||out->path!="/"))return false;
  out->expires=ageSet?(age<=0?0:now+age):
    expires==std::numeric_limits<double>::infinity()?expires:std::min(expires,now+400.0*86400);
  return true;
}
}
