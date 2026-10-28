// Application adaptation only: DOM state, resources and the task queue keep ownership.
// A dynamically inserted external classic script is fetched in the background
// (one speculative request on the Context's asynchronous transport, the same
// table parser scripts use) and delivered by a task once its response arrived,
// so the inserting script and other fetches are not held for the network, as
// in Firefox. Delivery order is completion order (force-async scripts). A
// frame's fetch runs on its own request context's source. With an enforced CSP
// the original synchronous preparation is kept unchanged.
#include "runtime/page_load.h"
#include "runtime/browsing_context.h"
#include "document_model/realm.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "execution_scheduling/realm.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallArgs.h"
#include "js/CharacterEncoding.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "domain/document_model/script_state.h"
#include "resource_network/page_http.h"
namespace ruyijs {
namespace {
enum Slot { Page, Script, Code, Filename, Document, Environment, Events, Generation, Loader, Url, Count };
// How long a delivery task waits before looking at its in-flight fetch again.
constexpr double kFetchPollMs = 2;
const JSClass kTask={"Script load task",JSCLASS_HAS_RESERVED_SLOTS(Count)};
}
bool PageLoad::ScriptPort(void* context,document_model::Realm& document,interaction::Realm& events,
    browser_environment::Environment& environment,JS::HandleObject page,JS::HandleObject script,bool external) {
  auto& loader=*static_cast<PageLoad*>(context);
  auto* target = loader.Find(document);
  if (!target) {
    JS_ReportErrorASCII(loader.cx_, "Script document owner is not registered"); return false;
  }
  Owners owners{loader.cx_,page,environment,document,events,loader.Network(*target),*target->platform};
  return loader.Dynamic(owners,script,external);
}
bool PageLoad::Dynamic(Owners& owners,JS::HandleObject script,bool external) {
  auto* cx=owners.cx;
  JSAutoRealm realm(cx,owners.page);
  JS::RootedValue href(cx);
  if(!owners.environment.URL(&href)||!href.isString())return false;
  JS::RootedString baseValue(cx,href.toString()),code(cx);
  std::string base,filename,url;bool defer=false,failed=false;
  if(!ToUTF8(cx,baseValue,&base))return false;
  resource_network::Realm* frame=nullptr;
  if(BrowsingContext* context=Find(owners.document);context&&context->resources&&
     &context->resources->Resources()!=&resources_)frame=context->resources.get();
  const bool background=external&&DynamicPreloadUrl(owners,script,base,&url)&&
      (StartPreload(url,frame),PreloadPending(PreloadKey(owners.resources,url)));
  if(!background&&!Source(owners,script,base,&code,&filename,&defer,&failed))return false;
  if(!external){uint32_t count=0;return !code||Execute(owners,script,code,filename,&count,1,{},"injectedScript");}
  JS::RootedObject data(cx,JS_NewObjectWithGivenProto(cx,&kTask,nullptr));
  JS::RootedString name(cx,JS_NewStringCopyN(cx,filename.data(),filename.size()));
  JS::RootedFunction fn(cx,js::NewFunctionWithReserved(cx,DeliverDynamic,0,0,"Script load task"));
  if(!data||!name||!fn)return false;
  JS::SetReservedSlot(data,Page,JS::ObjectValue(*owners.page));
  JS::SetReservedSlot(data,Script,JS::ObjectValue(*script));
  JS::SetReservedSlot(data,Code,code?JS::StringValue(code):JS::NullValue());
  JS::SetReservedSlot(data,Filename,JS::StringValue(name));
  JS::SetReservedSlot(data,Document,JS::PrivateValue(&owners.document));
  JS::SetReservedSlot(data,Environment,JS::PrivateValue(&owners.environment));
  JS::SetReservedSlot(data,Events,JS::PrivateValue(&owners.events));
  JS::SetReservedSlot(data,Generation,JS::DoubleValue(owners.document.ScriptGeneration()));
  JS::SetReservedSlot(data,Loader,JS::PrivateValue(this));
  // The task polls its preload by key; Source takes it by the same key.
  const std::string polled=PreloadKey(owners.resources,url);
  JS::RootedString pending(cx,JS_NewStringCopyN(cx,polled.data(),polled.size()));
  if(!pending)return false;
  JS::SetReservedSlot(data,Url,background?JS::StringValue(pending):JS::NullValue());
  JS::RootedObject callback(cx,JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(callback,0,JS::ObjectValue(*data));
  return scheduling_.PostTask(callback);
}
bool PageLoad::DeliverDynamic(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  // rval() shares the callee slot: keep the callee (re-posted while the fetch
  // is in flight) before writing the result.
  JS::RootedObject self(cx,&args.callee());
  JS::RootedObject data(cx,&js::GetFunctionNativeReserved(self,0).toObject());
  args.rval().setUndefined();
  auto& loader=*static_cast<PageLoad*>(JS::GetReservedSlot(data,Loader).toPrivate());
  JS::RootedObject page(cx,&JS::GetReservedSlot(data,Page).toObject());
  JS::RootedObject script(cx,&JS::GetReservedSlot(data,Script).toObject());
  // The task may wait several turns for its fetch, so the stored document
  // pointer is only compared, never dereferenced: a document that is no
  // longer registered, or whose global retired, has no script left to run.
  auto* stored=static_cast<const document_model::Realm*>(JS::GetReservedSlot(data,Document).toPrivate());
  auto* target=loader.Find(*stored);
  if(!target||target->page.get()!=page.get()||loader.scheduling_.Retired(page))return true;
  auto& document=*target->document;
  if(document.ScriptGeneration()!=uint64_t(JS::GetReservedSlot(data,Generation).toNumber()))return true;
  auto& environment=*target->environment;
  auto& events=*target->interaction;
  JSAutoRealm realm(cx,page);
  Owners owners=loader.OwnersOf(*target,page);
  JS::RootedValue url(cx,JS::GetReservedSlot(data,Url));
  if(url.isString()){
    // The background fetch: wait (without blocking the loop) for its response,
    // then prepare the element through the one Source path, which takes it.
    std::string reference;
    JS::RootedString text(cx,url.toString());
    if(!ToUTF8(cx,text,&reference))return false;
    if(loader.PreloadPending(reference))return loader.scheduling_.PostDelayedTask(self,kFetchPollMs);
    JS::RootedValue href(cx);
    if(!environment.URL(&href)||!href.isString())return false;
    JS::RootedString baseValue(cx,href.toString()),code(cx);
    std::string base,filename;bool defer=false,failed=false;
    if(!ToUTF8(cx,baseValue,&base)||!loader.Source(owners,script,base,&code,&filename,&defer,&failed))return false;
    if(!code)return !failed||events.Fire(script,"error",false,false);
    uint32_t count=0;
    return loader.Execute(owners,script,code,filename,&count,2)&&events.Fire(script,"load",false,false);
  }
  JS::RootedValue value(cx,JS::GetReservedSlot(data,Code));
  if(!value.isString())return events.Fire(script,"error",false,false);
  JS::RootedString code(cx,value.toString()),name(cx,JS::GetReservedSlot(data,Filename).toString());
  std::string filename;uint32_t count=0;
  return ToUTF8(cx,name,&filename)&&loader.Execute(owners,script,code,filename,&count,2)&&
      events.Fire(script,"load",false,false);
}
bool PageLoad::DynamicPreloadUrl(Owners& owners,JS::HandleObject script,const std::string& base,std::string* url){
  BrowsingContext* context=Find(owners.document);
  if(!transport_||!context||(context->security&&context->security->Enforcing()))return false;
  JSContext* cx=owners.cx;
  JS::RootedValue type(cx),skip(cx),src(cx);
  std::string text,reference;
  if(!owners.document.ScriptAttribute(script,u"type",&type)||!owners.document.ScriptAttribute(script,u"nomodule",&skip)||
     !owners.document.ScriptAttribute(script,u"src",&src))return false;
  if(type.isString()){JS::RootedString value(cx,type.toString());if(!ToUTF8(cx,value,&text))return false;}
  // Classic scripts only (a module is fetched by the module owner).
  if(!ruyijs::domain::document_model::script_classic<char>(text)||!skip.isNull()||!src.isString())return false;
  JS::RootedString value(cx,src.toString());
  if(!ToUTF8(cx,value,&reference)||reference.empty())return false;
  if(reference.rfind("//",0)==0)reference=base.substr(0,base.find("://")+1)+reference;
  return resource_network::ResolveURL(base,reference,url);
}
}
