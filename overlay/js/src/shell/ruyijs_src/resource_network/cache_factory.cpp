#include "resource_network/cache.h"
#include "resource_network/request.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "domain/browser_environment/url_value.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/PropertyAndElement.h"
#include <memory>
#include <new>

namespace ruyijs::resource_network {
namespace {
void FinalizeCache(JS::GCContext*,JSObject* self) {
  auto value=JS::GetReservedSlot(self,CNative);
  if (!value.isUndefined()) DeleteCacheHandle(static_cast<CacheHandle*>(value.toPrivate()));
}
void FinalizeStorage(JS::GCContext*,JSObject* self) {
  auto value=JS::GetReservedSlot(self,CNative);
  if (!value.isUndefined()) DeleteCacheScope(static_cast<CacheScope*>(value.toPrivate()));
}
const JSClassOps kCacheOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,FinalizeCache};
const JSClassOps kStorageOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,FinalizeStorage};
bool Illegal(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
  return Error(cx,"Illegal constructor.");
}
bool Caches(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  State& state=*static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
  if (self!=state.page) return Error(cx,"'get caches' called on an incompatible receiver.");
  if (!state.cacheStorageObject) {
    JSAutoRealm realm(cx,state.page);
    JS::RootedObject object(cx,state.kit.New(&kCacheStorageClass,state.cacheStorage.prototype)),factories(cx);
    if (!object || !StreamFactories(cx,state.page,&factories)) return false;
    CacheScope* scope=nullptr;
    if (!OpenRealmCacheScope(state,&scope)) return false;
    JS::SetReservedSlot(object,CNative,JS::PrivateValue(scope));
    JS::SetReservedSlot(object,CFactories,JS::ObjectValue(*factories));
    state.cacheStorageObject=object;
  }
  args.rval().setObject(*state.cacheStorageObject);return JS_WrapValue(cx,args.rval());
}
}
const JSClass kCacheClass={"Cache",JSCLASS_HAS_RESERVED_SLOTS(CSlots)|JSCLASS_FOREGROUND_FINALIZE,&kCacheOps};
const JSClass kCacheStorageClass={"CacheStorage",JSCLASS_HAS_RESERVED_SLOTS(CSlots)|JSCLASS_FOREGROUND_FINALIZE,&kStorageOps};
bool CacheNew(State& state,CacheHandle* handle,JS::MutableHandleObject result) {
  JS::RootedObject object(state.cx,state.kit.New(&kCacheClass,state.cache.prototype)),factories(state.cx);
  if (!object || !StreamFactories(state.cx,state.page,&factories)) return false;
  JS::SetReservedSlot(object,CNative,JS::PrivateValue(handle));
  JS::SetReservedSlot(object,CFactories,JS::ObjectValue(*factories));result.set(object);return true;
}
bool CacheResponseNew(State& state,const CacheResponseDTO& dto,JS::MutableHandleObject result) {
  JSContext* cx=state.cx;
  std::unique_ptr<BodyRecord> body(new(std::nothrow) BodyRecord);
  if (!body) {JS_ReportOutOfMemory(cx);return false;}
  body->bytes=dto.body;body->kind=dto.kind;body->nullBody=dto.nullBody;
  JS::RootedObject headers(cx);
  JS::RootedString status(cx,DecodeBodyText(cx,dto.statusText)),url(cx,DecodeBodyText(cx,dto.url));
  if (!status || !url || !HeaderObject(state,dto.headers,&headers,true) ||
      !ResponseNew(state,state.responsePrototype,body.get(),headers,dto.status,status,url,
          dto.type.c_str(),dto.redirected,result)) return false;
  body.release();return true;
}
bool InstallCache(State& state,PageHelpers& helpers,JS::HandleObject target) {
  JSContext* cx=state.cx;auto& kit=state.kit;
  JS::RootedObject parent(cx,JS::GetRealmObjectPrototype(cx));
  if (!kit.Define("Cache",&kCacheClass,parent,Illegal,&state.cache) ||
      !kit.Define("CacheStorage",&kCacheStorageClass,parent,Illegal,&state.cacheStorage)) return false;
  const char* cacheNames[]={"match","matchAll","add","addAll","put","delete","keys"};
  const unsigned lengths[]={1,0,1,1,2,1,0};
  for (int i=0;i<7;++i)
    if (!kit.Method(state.cache,"Cache",cacheNames[i],CacheCall,lengths[i],i)) return false;
  const char* storageNames[]={"match","has","open","delete","keys"};
  for (int i=0;i<5;++i)
    if (!kit.Method(state.cacheStorage,"CacheStorage",storageNames[i],CacheStorageCall,i==4?0:1,i)) return false;
  // WebIDL places constructor after prototype operations in property order.
  for (auto* face:{&state.cache,&state.cacheStorage}) {
    JS::RootedObject prototype(cx,face->prototype);
    if (!JS_DeleteProperty(cx,prototype,"constructor") ||
        !JS_DefineProperty(cx,prototype,"constructor",face->constructor,0)) return false;
  }
  JS::RootedObject getter(cx,kit.Function(Caches,"get caches",0,0)),setter(cx);
  return getter && JS_DefineProperty(cx,target,"caches",getter,setter,JSPROP_ENUMERATE) &&
      helpers.Accessor(state.workerScope?"WorkerGlobalScope":"Window",target,"caches") &&
      state.secure.Add(target,"caches") && state.secure.Add(state.page,"Cache") &&
      state.secure.Add(state.page,"CacheStorage");
}
}
