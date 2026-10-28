#include "resource_network/state.h"
#include "resource_network/page_cookie_store.h"
#include "runtime/page_helpers.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::resource_network {
namespace {
bool Context(void* opaque,HttpContext* out) {
  auto& s=*static_cast<State*>(opaque);
  if(s.httpContext&&!s.httpContext(s.httpEnvironment,out))return false;
  out->now=s.cookieClock?s.cookieClock(s.cookieClockContext):0;
  return true;
}
bool Address(State& s,const JS::CallArgs& args,std::string* url) {
  JS::RootedValue title(s.cx);
  // Original DOM native getter supplies Document brand validation, including
  // borrowed receivers. No script-visible property is read at access time.
  if(!JS::Call(s.cx,args.thisv(),s.cookieBrand,JS::HandleValueArray::empty(),&title))return false;
  return s.cookieDocument&&s.cookieDocument(s.httpEnvironment,args.thisv(),url);
}
bool Get(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);State& s=*FunctionOwner(args);
  std::string url,joined;
  if(!Address(s,args,&url))return false;
  s.Observe("Document","cookie",int(ApiAccess::Read));
  const double now=s.cookieClock?s.cookieClock(s.cookieClockContext):0;
  if(!Report(cx,CookieJoined(s.store,url,now,false,&joined)))return false;
  JS::RootedString value(cx,DecodeBodyText(cx,joined));
  if(!value)return false;args.rval().setString(value);return true;
}
bool Set(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);State& s=*FunctionOwner(args);
  std::string url,text;
  if(!Address(s,args,&url))return false;
  if(!argc)return Error(cx,"Document.cookie setter requires a value");
  if(!EncodeBodyText(cx,args[0],&text))return false;
  s.Observe("Document","cookie",int(ApiAccess::Write));
  const double now=s.cookieClock?s.cookieClock(s.cookieClockContext):0;
  // A document.cookie write changes the one jar, so window.cookieStore reports it
  // like its own set: its change event is queued as this realm's next task.
  CookieChangeSet change;
  if(!Report(cx,CookieAssign(s.store,url,text,now,false,"",false,&change)))return false;
  if(!PostCookieChange(s,change))return false;
  args.rval().setUndefined();return true;
}
}
void Realm::SetHttpEnvironment(HttpContextPort environment,CookieDocumentPort document,
    void* context,CookieClockPort clock,void* clockContext) {
  if(!state_)return;
  state_->httpContext=environment;state_->cookieDocument=document;state_->httpEnvironment=context;
  state_->cookieClock=clock;state_->cookieClockContext=clockContext;
  state_->resources.SetHttpContext(state_->store,environment?Context:nullptr,environment?state_.get():nullptr);
}
bool InstallCookie(State& s,PageHelpers& helpers,JS::HandleObject prototype) {
  JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> title(s.cx);
  if(!JS_GetOwnPropertyDescriptor(s.cx,prototype,"title",&title)||title.isNothing()||!title->getter())return false;
  s.cookieBrand.setObject(*title->getter());
  JS::RootedObject get(s.cx,NativeFunction(s,Get,"get cookie",0));
  JS::RootedObject set(s.cx,NativeFunction(s,Set,"set cookie",1));
  return get&&set&&JS_DefineProperty(s.cx,prototype,"cookie",get,set,JSPROP_ENUMERATE)&&
    helpers.Member(get,"Document","get cookie","get")&&helpers.Member(set,"Document","set cookie","set");
}
}
