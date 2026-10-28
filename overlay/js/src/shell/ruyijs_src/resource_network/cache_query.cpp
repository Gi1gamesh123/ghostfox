#include "resource_network/cache.h"
#include "resource_network/request.h"
#include "js/Array.h"
#include "js/Promise.h"
#include "js/Realm.h"
#include "js/String.h"

namespace ruyijs::resource_network {
bool CacheQueryCall(State& state,JS::HandleObject self,const JS::CallArgs& args,
                    CacheMethod method,const CacheQueryDTO& query) {
  JSContext* cx=state.cx;
  JSAutoRealm realm(cx,self);
  JS::RootedValue value(cx);
  if (method==CacheMethod::Delete) {
    bool removed=false;
    if (!CacheReport(cx,CacheDelete(CacheNative(self),query,&removed))) return false;
    value.setBoolean(removed);
  } else {
    CacheResults results;
    if (!CacheReport(cx,CacheQuery(CacheNative(self),query,&results))) return false;
    JS::RootedObject list(cx),object(cx);
    if (method==CacheMethod::Match) {
      if (!results.responses.empty()) {
        if (!CacheResponseNew(state,results.responses.front(),&object)) return false;
        value.setObject(*object);
      }
    } else {
      list=JS::NewArrayObject(cx,0);
      if (!list) return false;
      uint32_t index=0;
      for (const auto& response:results.responses) {
        if (!CacheResponseNew(state,response,&object) || !JS_SetElement(cx,list,index++,object)) return false;
      }
      for (const auto& request:results.requests) {
        if (!RequestNew(state,state.requestPrototype,request,JS::NullHandleValue,&object) ||
            !JS_SetElement(cx,list,index++,object)) return false;
      }
      value.setObject(*list);
    }
  }
  return Settled(cx,value,args.rval());
}
namespace {
bool StorageOperation(State& state,JS::HandleObject self,const JS::CallArgs& args,
                       CacheStorageMethod method,const std::u16string& name,
                       const CacheStorageQueryDTO& query) {
  JSContext* cx=state.cx;
  JSAutoRealm realm(cx,self);
  CacheScope* scope=CacheStorageNative(self);
  JS::RootedValue value(cx);
  if (method==CacheStorageMethod::Open) {
    CacheHandle* handle=nullptr;
    if (!CacheReport(cx,CacheOpen(scope,name,&handle))) return false;
    JS::RootedObject object(cx);
    if (!CacheNew(state,handle,&object)) {DeleteCacheHandle(handle);return false;}
    value.setObject(*object);
  } else if (method==CacheStorageMethod::Keys) {
    std::vector<std::u16string> names;
    if (!CacheReport(cx,CacheNames(scope,&names))) return false;
    JS::RootedObject array(cx,JS::NewArrayObject(cx,0));
    if (!array) return false;
    for (size_t i=0;i<names.size();++i) {
      JS::RootedString string(cx,JS_NewUCStringCopyN(cx,names[i].data(),names[i].size()));
      if (!string || !JS_SetElement(cx,array,uint32_t(i),string)) return false;
    }
    value.setObject(*array);
  } else if (method==CacheStorageMethod::Match) {
    CacheResponseDTO response;bool found=false;
    if (!CacheReport(cx,CacheStorageMatch(scope,query,&response,&found))) return false;
    if (found) {
      JS::RootedObject object(cx);
      if (!CacheResponseNew(state,response,&object)) return false;
      value.setObject(*object);
    }
  } else {
    bool result=false;
    const auto status=method==CacheStorageMethod::Has?CacheHas(scope,name,&result):CacheRemoveName(scope,name,&result);
    if (!CacheReport(cx,status)) return false;
    value.setBoolean(result);
  }
  return Settled(cx,value,args.rval());
}
}
bool CacheStorageCall(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  const auto method=CacheStorageMethod(WebKit::Slot(args));
  const char* names[]={"match","has","open","delete","keys"};
  const char* name=names[int(method)];
  JS::RootedObject self(cx);
  const std::string error=std::string("'")+name+"' called on an object that does not implement interface CacheStorage.";
  self=args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr;
  if (!self || JS::GetClass(self)!=&kCacheStorageClass) {
    Error(cx,error.c_str());return CacheRejectPending(cx,args.rval());
  }
  if (!argc && method!=CacheStorageMethod::Keys) {
    CacheBindingError(cx,"CacheStorage",name,"At least 1 argument required, but only 0 passed");
    return CacheRejectPending(cx,args.rval());
  }
  State* state=CacheState(cx,self);
  if (!state) return false;
  state->Observe("CacheStorage",name,int(ApiAccess::Call));
  std::u16string storeName;CacheStorageQueryDTO query;
  if (method==CacheStorageMethod::Match) {
    bool request=false;std::string text;
    query.query.hasRequest=true;
    if (!CacheRequestInfo(*state,args.get(0),&query.query.request,&text,&request) ||
        !CacheOptions(cx,args.get(1),&query,true,name) ||
        !CacheResolveRequest(*state,&query.query.request,text,request,name,"CacheStorage")) return CacheRejectPending(cx,args.rval());
  } else if (method!=CacheStorageMethod::Keys && !CacheName(cx,args.get(0),&storeName))
    return CacheRejectPending(cx,args.rval());
  if (!StorageOperation(*state,self,args,method,storeName,query)) return CacheRejectPending(cx,args.rval());
  return JS_WrapValue(cx,args.rval());
}
}
