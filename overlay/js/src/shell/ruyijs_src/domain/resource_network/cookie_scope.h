#pragma once
#include "domain/browser_environment/url_value.h"
#include "domain/resource_network/cookie_suffix_data.h"
#include <algorithm>
#include <string>
#include <string_view>

namespace ruyijs::domain::resource_network {
using CookieURL = browser_environment::UrlValue;
inline bool CookieIP(const std::string& host) {
  return host.find(':')!=std::string::npos || (!host.empty() &&
    std::all_of(host.begin(),host.end(),[](char c){return (c>='0'&&c<='9')||c=='.';}));
}
inline bool CookieDomain(const std::string& host,const std::string& domain) {
  return host==domain || (!CookieIP(host)&&host.size()>domain.size()&&
    host.compare(host.size()-domain.size(),domain.size(),domain)==0&&host[host.size()-domain.size()-1]=='.');
}
inline bool CookiePath(const std::string& path,const std::string& prefix) {
  return path==prefix || (path.size()>prefix.size()&&path.compare(0,prefix.size(),prefix)==0&&
    (prefix.back()=='/'||path[prefix.size()]=='/'));
}
inline bool CookieSuffixRule(std::string_view rule) {
  return std::binary_search(std::begin(kCookieSuffixRules),std::end(kCookieSuffixRules),rule);
}
inline std::string CookieSuffix(const std::string& host) {
  size_t suffix=host.rfind('.');suffix=suffix==std::string::npos?0:suffix+1;
  for(size_t i=0;;) {
    std::string part=host.substr(i);
    if(CookieSuffixRule("!"+part)) { auto dot=host.find('.',i);return host.substr(dot+1); }
    if(CookieSuffixRule(part))suffix=std::min(suffix,i);
    auto dot=host.find('.',i);
    if(dot==std::string::npos)break;
    if(CookieSuffixRule("*"+host.substr(dot)))suffix=std::min(suffix,i);
    i=dot+1;
  }
  return host.substr(suffix);
}
inline std::string CookieSite(const CookieURL& url) {
  const auto& host=url.hostname();
  if(CookieIP(host))return url.protocol()+host;
  auto suffix=CookieSuffix(host);
  size_t start=host.size()-suffix.size();
  if(start) {auto dot=host.rfind('.',start-2);start=dot==std::string::npos?0:dot+1;}
  return url.protocol()+host.substr(start);
}
inline bool CookieSecure(const CookieURL& url) {
  return url.protocol()=="https:" || url.hostname()=="localhost" ||
    url.hostname()=="127.0.0.1" || url.hostname()=="[::1]";
}
}
