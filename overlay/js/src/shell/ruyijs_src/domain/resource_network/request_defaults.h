#pragma once
#include "domain/resource_network/referrer.h"
#include "domain/resource_network/cookie_scope.h"
#include <utility>
#include <vector>

namespace ruyijs::domain::resource_network {
using RequestHeaders=std::vector<std::pair<std::string,std::string>>;
struct RequestDefaults {
  std::string source,target,userAgent,referrer,policy,initiator,mode,method;
  std::vector<std::string> languages;
  bool navigation=false,cors=false;
  std::string redirectSite;
  // The top-level document URL and whether the request includes credentials:
  // Firefox's Storage Access Headers (nsHttpChannel::AddStorageAccessHeadersToRequest)
  // send "Sec-Fetch-Storage-Access: none" on a credentialed secure request that
  // is third-party to the top-level site (trace 20261021-160155: the captcha
  // iframe and its scripts carry it; same-origin XHR and beacons do not).
  std::string topLevel;
  bool credentialed=false;
  // An XHR sent while handling user input is UrgentStart: urgency 1 with the
  // XHR adjustment, "u=0" on the wire (trace 20261021-160155 /captcha/check).
  bool urgent=false;
  // A navigation the user activated: Sec-Fetch-User "?1" (Fetch Metadata).
  bool userActivated=false;
};
inline void DefaultHeader(RequestHeaders& headers,const std::string& name,const std::string& value) {
  if(value.empty())return;
  for(const auto& h:headers)if(lower_header_name(h.first)==lower_header_name(name))return;
  headers.emplace_back(name,value);
}
inline std::string RequestSite(const std::string& from,const std::string& target) {
  CookieURL source(from),url(target);
  if(source.protocol()!="http:"&&source.protocol()!="https:")return "none";
  if(source.origin()==url.origin())return "same-origin";
  return CookieSite(source)==CookieSite(url)?"same-site":"cross-site";
}
inline std::string DefaultReferrer(const RequestDefaults& d) {
  std::string policy=d.policy;
  // Firefox's default ETP setting ignores relaxing policies on cross-site
  // subresources (network.http.referer.disallowCrossSiteRelaxingDefault).
  if(!d.navigation&&RequestSite(d.source,d.target)=="cross-site"&&
      (policy=="unsafe-url"||policy=="no-referrer-when-downgrade"||policy=="origin-when-cross-origin"))
    policy="strict-origin-when-cross-origin";
  return RequestReferrer(d.referrer,d.target,policy);
}
inline void AddRequestDefaults(const RequestDefaults& d,RequestHeaders& headers) {
  DefaultHeader(headers,"User-Agent",d.userAgent);
  std::string language;
  for(size_t i=0;i<d.languages.size()&&i<10;++i) {
    if(d.languages[i].empty()||!valid_header_value(d.languages[i]))continue;
    if(!language.empty())language+=',';
    language+=d.languages[i];
    if(i)language+=";q=0."+std::to_string(10-i);
  }
  DefaultHeader(headers,"Accept-Language",language);
  std::string destination="empty",mode=d.mode.empty()?"cors":d.mode,accept="*/*";
  if(d.navigation||d.initiator=="iframe") {
    destination=d.initiator=="iframe"?"iframe":"document";mode="navigate";
    accept="text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
    DefaultHeader(headers,"Upgrade-Insecure-Requests","1");
  } else if(d.initiator=="script") {destination="script";mode=d.cors?"cors":"no-cors";}
  else if(d.initiator=="link") {destination="style";mode=d.cors?"cors":"no-cors";accept="text/css,*/*;q=0.1";}
  else if(d.initiator=="img") {
    destination="image";mode=d.cors?"cors":"no-cors";
    accept="image/avif,image/webp,image/png,image/svg+xml,image/*;q=0.8,*/*;q=0.5";
  } else if(d.initiator=="beacon")mode=d.cors?"cors":"no-cors";
  DefaultHeader(headers,"Accept",accept);
  // These fields are part of Firefox's wire profile.  HTTP/2 and HTTP/3
  // remove connection-specific fields at their protocol boundary.
  DefaultHeader(headers,"Accept-Encoding","gzip, deflate, br, zstd");
  DefaultHeader(headers,"Connection","keep-alive");
  std::string priority;
  if (d.navigation) priority="u=0, i";
  else if (d.initiator=="beacon") priority="u=6";
  else if (d.initiator=="link") priority="u=2";
  else if (d.initiator=="img") priority="u=4, i";
  else if (d.initiator=="iframe"||d.initiator=="fetch") priority="u=4";
  else if (d.initiator=="xmlhttprequest"&&d.urgent) priority="u=0";
  DefaultHeader(headers,"Priority",priority);
  DefaultHeader(headers,"Referer",DefaultReferrer(d));
  CookieURL url(d.target);
  // Firefox emits Fetch Metadata only to potentially trustworthy origins.
  if(CookieSecure(url)) {
    DefaultHeader(headers,"Sec-Fetch-Dest",destination);
    DefaultHeader(headers,"Sec-Fetch-Mode",mode);
    std::string site=RequestSite(d.source,d.target);
    if(d.redirectSite=="cross-site"||site=="none")site=d.redirectSite.empty()?site:d.redirectSite;
    else if(d.redirectSite=="same-site"&&site=="same-origin")site="same-site";
    DefaultHeader(headers,"Sec-Fetch-Site",site);
    if(d.navigation&&d.userActivated)DefaultHeader(headers,"Sec-Fetch-User","?1");
    if(d.credentialed&&!d.navigation&&d.initiator!="beacon"&&!d.topLevel.empty()) {
      CookieURL top(d.topLevel);
      if((top.protocol()=="http:"||top.protocol()=="https:")&&CookieSite(top)!=CookieSite(url))
        DefaultHeader(headers,"Sec-Fetch-Storage-Access","none");
    }
  }
}
}
