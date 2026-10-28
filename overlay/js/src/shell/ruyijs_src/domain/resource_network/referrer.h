#pragma once
#include "domain/browser_environment/url_value.h"
#include "domain/resource_network/header_rules.h"
#include <string>

namespace ruyijs::domain::resource_network {
inline bool ReferrerPolicy(const std::string& value) {
  return value=="no-referrer"||value=="no-referrer-when-downgrade"||value=="origin"||
    value=="origin-when-cross-origin"||value=="same-origin"||value=="strict-origin"||
    value=="strict-origin-when-cross-origin"||value=="unsafe-url";
}
inline void ReadReferrerPolicy(const std::string& header,std::string* policy) {
  for(size_t start=0;start<header.size();) {
    auto end=header.find(',',start);
    auto value=std::string(trim_header_value(header.substr(start,end==std::string::npos?end:end-start)));
    if(ReferrerPolicy(value))*policy=value;
    if(end==std::string::npos)break;start=end+1;
  }
}
inline std::string RequestReferrer(const std::string& source,const std::string& target,
                                  const std::string& requestedPolicy) {
  using URL=browser_environment::UrlValue;
  URL from(source),to(target);
  if((from.protocol()!="http:"&&from.protocol()!="https:")||requestedPolicy=="no-referrer")return {};
  from.set_username("");from.set_password("");from.set_hash("");
  const bool same=from.origin()==to.origin();
  const bool downgrade=from.protocol()=="https:"&&to.protocol()!="https:";
  const std::string policy=ReferrerPolicy(requestedPolicy)?requestedPolicy:"strict-origin-when-cross-origin";
  if(policy=="same-origin"&&!same)return {};
  if(downgrade&&(policy=="no-referrer-when-downgrade"||policy=="strict-origin"||
                 policy=="strict-origin-when-cross-origin"))return {};
  if(policy=="origin"||policy=="strict-origin"||(!same&&
      (policy=="origin-when-cross-origin"||policy=="strict-origin-when-cross-origin"))||from.href().size()>4096)
    return from.origin()+"/";
  return from.href();
}
}
