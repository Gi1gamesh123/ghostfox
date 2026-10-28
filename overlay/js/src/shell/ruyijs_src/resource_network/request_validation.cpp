// Request semantic validation after the single WebIDL conversion boundary.
#include "resource_network/request.h"
#include "resource_network/state.h"
#include "domain/browser_environment/url_value.h"
#include <cctype>

namespace ruyijs::resource_network {
namespace {
using URL=ruyijs::domain::browser_environment::UrlValue;
bool NormalizeMethod(JSContext* cx, std::string* method) {
  if (method->empty()) return Error(cx,"Invalid HTTP method");
  for (unsigned char c:*method) {
    if ((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')) continue;
    if (std::string("!#$%&'*+-.^_`|~").find(char(c))==std::string::npos)
      return Error(cx,"Invalid HTTP method");
  }
  std::string upper=*method;
  for(char& c:upper)c=char(std::toupper(static_cast<unsigned char>(c)));
  if(upper=="CONNECT"||upper=="TRACE"||upper=="TRACK")return Error(cx,"Forbidden HTTP method");
  for(const auto* name:{"DELETE","GET","HEAD","OPTIONS","POST","PUT"})
    if(upper==name){*method=upper;break;}
  return true;
}
}
bool RequestURL(State& state, const std::string& text, std::string* out) {
  HttpContext context;
  if(!state.resources.ReadHttpContext(&context))return false;
  URL url;
  if(!URL::Resolve(text,context.source,&url))return Error(state.cx,"Request URL is invalid");
  if(!url.username().empty()||!url.password().empty())return Error(state.cx,"Request URL has credentials");
  *out=url.href();return true;
}
bool ApplyRequestInit(State& state, const RequestInitFields& init, RequestDraft* draft) {
  auto& d=*draft;const auto& v=init.values;const uint32_t p=init.present;
  if(p){d.referrer="about:client";d.referrer_policy.clear();}
  if(p&InitCache)d.cache=v.cache;
  if(p&InitCredentials)d.credentials=v.credentials;
  if(p&InitHeaders)d.headers=v.headers;
  if(p&InitIntegrity)d.integrity=v.integrity;
  if(p&InitKeepalive)d.keepalive=v.keepalive;
  if(p&InitMethod)d.method=v.method;
  if(p&InitMode)d.mode=v.mode;
  if(p&InitRedirect)d.redirect=v.redirect;
  if(p&InitReferrerPolicy)d.referrer_policy=v.referrer_policy;
  if(p&InitReferrer){
    d.referrer=v.referrer;
    if(!d.referrer.empty()){
      HttpContext context;URL referrer;
      if(!state.resources.ReadHttpContext(&context))return false;
      if(!URL::Resolve(d.referrer,context.source,&referrer))return Error(state.cx,"Invalid referrer URL");
      const URL source(context.source);
      d.referrer=referrer.href()=="about:client"||
          (source.origin()!="null"&&source.origin()==referrer.origin())?referrer.href():"about:client";
    }
  }
  if((p&InitMode)&&d.mode=="navigate")return Error(state.cx,"Invalid Request mode");
  if((p&InitCache)&&d.cache=="only-if-cached"&&d.mode!="same-origin")
    return Error(state.cx,"only-if-cached requires same-origin mode");
  if((p&InitMethod)&&!NormalizeMethod(state.cx,&d.method))return false;
  if(d.mode=="no-cors"&&d.method!="GET"&&d.method!="HEAD"&&d.method!="POST")
    return Error(state.cx,"Invalid no-cors method");
  if(!v.nullBody){d.body=v.body;d.kind=v.kind;d.nullBody=false;}
  if(!d.nullBody&&(d.method=="GET"||d.method=="HEAD"))
    return Error(state.cx,"Request with GET or HEAD method cannot have a body");
  return true;
}
}
