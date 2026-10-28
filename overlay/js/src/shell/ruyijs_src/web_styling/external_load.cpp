// One external completion task, holding the original sheet in GC-traced slots.
#include "web_styling/cssom.h"
#include "web_styling/realm.h"
#include "domain/styling/external_style_value.h"
#include "js/Realm.h"
#include "js/CallAndConstruct.h"
#include "jsfriendapi.h"
namespace ruyijs::web_styling {
namespace {
std::string Lower(std::string value) {
  for (char& c:value) if (c>='A' && c<='Z') c += 'a'-'A';
  return value;
}
std::string Header(const StyleLoadResult& result, const char* name) {
  for (const auto& [key,value]:result.headers) if (Lower(key)==name) return value;
  return {};
}
bool CorsAllowed(const SheetCache& cache, const StyleLoadResult& result) {
  if (!cache.hasCors || cache.origin==cache.resourceOrigin) return true;
  const bool credentials=Lower(cache.sourceCors)=="use-credentials";
  const std::string allow=Header(result,"access-control-allow-origin");
  return (allow==cache.origin || (!credentials && allow=="*")) &&
      (!credentials || Header(result,"access-control-allow-credentials")=="true");
}
// The response of an asynchronous load: kept on the sheet, then the same task
// runs again (a cancelled sheet's task has no sheet left and does nothing).
void Arrived(JSContext* cx, JS::HandleObject task, StyleLoadResult&& response) {
  JS::RootedValue value(cx,js::GetFunctionNativeReserved(task,0));
  if (!value.isObject()) return;
  JS::RootedObject sheet(cx,&value.toObject());
  State& state=SheetOwnerOf(sheet);
  SheetDataOf(sheet).arrived=std::make_shared<StyleLoadResult>(std::move(response));
  JSAutoRealm realm(cx,state.page);
  if (!state.postTask || !state.postTask(state.taskContext,task)) JS_ClearPendingException(cx);
}
bool Task(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject self(cx,&args.callee());  // before rval(), which shares the callee slot
  JS::RootedValue value(cx,js::GetFunctionNativeReserved(self,0));
  args.rval().setUndefined();
  if (!value.isObject()) return true;
  JS::RootedObject sheet(cx,&value.toObject());
  State& state=SheetOwnerOf(sheet);
  JSAutoRealm realm(cx,state.page);
  auto& cache=SheetDataOf(sheet);
  if (!RefreshSheetSource(cx,sheet)) return false;
  if (cache.retired || !state.loadPort) { CancelExternalSheet(sheet); return true; }
  if (!cache.resource) {
    StyleLoadRequest request{cache.href,cache.documentURL,cache.origin,cache.hasCors};
    StyleLoadResult result;
    bool fetched=false;
    if (cache.arrived) {
      result=std::move(*cache.arrived);cache.arrived.reset();cache.requested=false;
      fetched=result.fetched;
    } else if (cache.requested) {
      return true;  // still in flight
    } else {
      // Load in parallel with the parser and the other sheets (Firefox loads
      // stylesheets asynchronously); the sheet stays loading meanwhile.
      if (state.startPort) {
        auto task=std::make_shared<JS::PersistentRootedObject>(cx,self);
        cache.requested=state.startPort(state.startContext,request,[cx,task](StyleLoadResult&& response) {
          JS::RootedObject callback(cx,*task);
          Arrived(cx,callback,std::move(response));
        });
        if (cache.requested) return true;
      }
      fetched=state.loadPort(state.loadContext,request,&result);
    }
    const bool cors=fetched && CorsAllowed(cache,result);
    const bool ok=fetched && result.error.empty() && result.status>=200 && result.status<300 &&
        cors && ruyijs::domain::styling::CssResponseMime(Header(result,"content-type"));
    auto resource=std::make_shared<ruyijs::domain::styling::ExternalStyleResource>();
    resource->url=cache.href;resource->documentURL=cache.documentURL;
    resource->origin=cache.origin;resource->mode=ExternalCorsMode(cache);
    resource->success=ok;resource->text=ok ? std::move(result.body) : std::string();
    resource->originClean=!ok || cache.origin==cache.resourceOrigin || (cache.hasCors && cors);
    cache.resource=resource;state.externalResources.Put(resource);
  }
  cache.originClean=cache.resource->originClean;
  if (!cache.ready && !ReplaceRuleRows(cx,sheet,cache.resource->text)) return false;
  cache.ready=true;cache.loadDelivered=true;
  CancelExternalSheet(sheet);
  JS::RootedValue node(cx,JS::GetReservedSlot(sheet,SheetOwnerNode));
  if (node.isObject() && state.loadEvent) {
    JS::RootedObject target(cx,&node.toObject());
    return state.loadEvent(state.loadEventContext,target,cache.resource->success ? "load" : "error");
  }
  return true;
}
}
void CancelExternalSheet(JS::HandleObject sheet) {
  const auto& value=JS::GetReservedSlot(sheet,SheetPendingLoad);
  if (value.isObject()) {
    js::SetFunctionNativeReserved(&value.toObject(),0,JS::UndefinedValue());
    JS::SetReservedSlot(sheet,SheetPendingLoad,JS::UndefinedValue());
  }
  auto& data=SheetDataOf(sheet);
  data.loading=false;data.requested=false;data.arrived.reset();
}
bool EnsureExternalSheet(State& state,const std::shared_ptr<SheetCache>& cache) {
  if (!cache->external || cache->loadDelivered || cache->loading) return true;
  if (!state.loadPort || !state.postTask) return true;
  JSContext* cx=state.cx;
  JS::RootedObject sheet(cx);
  if (!SheetObject(state,cache,&sheet)) return false;
  JS::RootedFunction fn(cx,js::NewFunctionWithReserved(cx,Task,0,0,nullptr));
  if (!fn) return false;
  JS::RootedObject callback(cx,JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(callback,0,JS::ObjectValue(*sheet));
  JS::SetReservedSlot(sheet,SheetPendingLoad,JS::ObjectValue(*callback));
  cache->loading=true;
  // The request starts now, as a browser starts it when the link is seen; the
  // load task runs when the response arrives.
  if (state.startPort && !cache->resource) {
    StyleLoadRequest request{cache->href,cache->documentURL,cache->origin,cache->hasCors};
    auto task=std::make_shared<JS::PersistentRootedObject>(cx,callback);
    cache->requested=state.startPort(state.startContext,request,[cx,task](StyleLoadResult&& response) {
      JS::RootedObject pending(cx,*task);
      Arrived(cx,pending,std::move(response));
    });
    if (cache->requested) return true;
  }
  if (state.postTask(state.taskContext,callback)) return true;
  CancelExternalSheet(sheet);return false;
}
std::string ExternalCorsMode(const SheetCache& cache) {
  return !cache.hasCors ? "no-cors" : Lower(cache.sourceCors)=="use-credentials" ?
      "use-credentials" : "anonymous";
}
void Realm::SetLoadPort(StyleLoadPort port,void* context) {
  state_->loadPort=port;state_->loadContext=context;
}
bool Realm::LoadsInFlight() const {
  for (const auto& sheet : state_->sheets)
    if (sheet && !sheet->retired && sheet->loading && sheet->requested && !sheet->arrived) return true;
  return false;
}
bool Realm::LoadsPending() const {
  for (const auto& sheet : state_->sheets)
    if (sheet && !sheet->retired && sheet->loading) return true;
  return false;
}
bool Realm::CompleteArrived() {
  JSContext* cx=state_->cx;
  for (size_t i=0;i<state_->sheets.size();++i) {
    std::shared_ptr<SheetCache> cache=state_->sheets[i];
    if (!cache || cache->retired || !cache->loading || !cache->arrived) continue;
    JS::RootedObject sheet(cx);
    if (!SheetObject(*state_,cache,&sheet)) return false;
    JS::RootedValue callback(cx,JS::GetReservedSlot(sheet,SheetPendingLoad)),ignored(cx);
    if (!callback.isObject()) continue;
    JSAutoRealm realm(cx,state_->page);
    if (!JS::Call(cx,JS::UndefinedHandleValue,callback,JS::HandleValueArray::empty(),&ignored)) return false;
  }
  return true;
}
void Realm::SetLoadStartPort(StyleLoadStartPort port,void* context) {
  state_->startPort=port;state_->startContext=context;
}
void Realm::SetLoadEventPort(StyleLoadEventPort port,void* context) {
  state_->loadEvent=port;state_->loadEventContext=context;
}
}
