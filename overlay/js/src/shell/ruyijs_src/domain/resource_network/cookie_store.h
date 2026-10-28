#pragma once
#include "domain/resource_network/cookie_date.h"
#include "domain/resource_network/cookie_scope.h"
#include <limits>
#include <vector>

namespace ruyijs::domain::resource_network {
enum class CookieSameSite { Default, Lax, Strict, None };
struct Cookie {
  std::string name,value,domain,path;
  bool hostOnly=true,secure=false,httpOnly=false;
  CookieSameSite sameSite=CookieSameSite::Default;
  double expires=std::numeric_limits<double>::infinity(),created=0;
  uint64_t order=0;
};
// One API-visible cookie: the name/value pair the CookieStore interface returns.
struct CookiePair { std::string name,value; };
// What one Assign() changed, for the CookieChangeEvent the script interface
// queues: the cookies written and the cookies removed, in the order touched.
struct CookieChanges { std::vector<CookiePair> changed,deleted; };
bool ParseCookie(const std::string&,const CookieURL&,double,Cookie*);
class CookieStore {
 public:
  // Host snapshot of the whole live jar, without URL or script visibility filters.
  std::vector<Cookie> Snapshot(double now) const {
    std::vector<Cookie> result;
    for (const auto& cookie : cookies_)
      if (cookie.expires > now) result.push_back(cookie);
    return result;
  }
  void Assign(const std::string& url,const std::string& text,double now,bool http,
      const std::string& source="",bool navigation=false,CookieChanges* changes=nullptr) {
    CookieURL address(url);Cookie candidate;
    if(!ParseCookie(text,address,now,&candidate)||(!http&&candidate.httpOnly))return;
    if(http&&!navigation&&!source.empty()&&CookieSite(address)!=CookieSite(CookieURL(source))&&
       candidate.sameSite!=CookieSameSite::None)return;
    Expire(now);
    auto same=[&](const Cookie& c){return c.name==candidate.name&&c.domain==candidate.domain&&c.path==candidate.path;};
    auto old=std::find_if(cookies_.begin(),cookies_.end(),same);
    if(old!=cookies_.end()&&!http&&old->httpOnly)return;
    if(!CookieSecure(address)&&!candidate.secure) {
      for(const auto& c:cookies_)if(c.secure&&c.name==candidate.name&&
        (CookieDomain(c.domain,candidate.domain)||CookieDomain(candidate.domain,c.domain))&&
        CookiePath(candidate.path,c.path))return;
    }
    // Firefox retains ordering for identical values, but a changed value gets
    // a new creation position (independent Firefox contract includes both).
    bool identical=old!=cookies_.end()&&old->value==candidate.value;
    const bool replaced=old!=cookies_.end();
    if(identical){candidate.order=old->order;candidate.created=old->created;}
    if(replaced)cookies_.erase(old);
    if(candidate.expires<=now) {
      // A past expiry removes the cookie; its change entry carries the name only.
      if(changes&&replaced)changes->deleted.push_back({candidate.name,{}});
      return;
    }
    // Firefox refreshes creation order when an existing cookie is replaced.
    if(!identical){candidate.order=++order_;candidate.created=now;}
    if(cookies_.size()>=3000)cookies_.erase(cookies_.begin());
    if(changes)changes->changed.push_back({candidate.name,candidate.value});
    cookies_.push_back(std::move(candidate));
  }
  std::string Joined(const std::string& url,double now,bool http,
      const std::string& source="",bool navigation=false,bool safe=true) {
    std::string result;
    for(const Cookie* c:Select(url,now,http,source,navigation,safe)) {
      if(!result.empty())result+="; ";result+=c->name+"="+c->value;
    }
    return result;
  }
  // The cookies a script-visible read returns for |url|: the same selection and
  // order Joined uses (non-httpOnly when |http| is false), as name/value pairs.
  std::vector<CookiePair> Matches(const std::string& url,double now,bool http,
      const std::string& source="",bool navigation=false) {
    std::vector<CookiePair> result;
    for(const Cookie* c:Select(url,now,http,source,navigation,true))
      result.push_back({c->name,c->value});
    return result;
  }
 private:
  std::vector<const Cookie*> Select(const std::string& url,double now,bool http,
      const std::string& source,bool navigation,bool safe) {
    Expire(now);CookieURL address(url),initiator(source);
    std::vector<const Cookie*> selected;
    if(address.protocol()!="http:"&&address.protocol()!="https:")return selected;
    const bool sameSite=source.empty()||CookieSite(address)==CookieSite(initiator);
    for(const auto& c:cookies_) {
      if((c.hostOnly?address.hostname()!=c.domain:!CookieDomain(address.hostname(),c.domain))||
         !CookiePath(address.pathname(),c.path)||(c.secure&&!CookieSecure(address))||(!http&&c.httpOnly))continue;
      if(http&&!sameSite&&c.sameSite!=CookieSameSite::None) {
        if(c.sameSite==CookieSameSite::Strict||!navigation||
           (!safe&&!(c.sameSite==CookieSameSite::Default&&now-c.created<=120)))continue;
      }
      selected.push_back(&c);
    }
    std::sort(selected.begin(),selected.end(),[](const Cookie* a,const Cookie* b){
      return a->path.size()!=b->path.size()?a->path.size()>b->path.size():a->order<b->order;
    });
    return selected;
  }
  void Expire(double now) {
    cookies_.erase(std::remove_if(cookies_.begin(),cookies_.end(),[&](const Cookie& c){return c.expires<=now;}),cookies_.end());
  }
  std::vector<Cookie> cookies_;
  uint64_t order_=0;
};
}
