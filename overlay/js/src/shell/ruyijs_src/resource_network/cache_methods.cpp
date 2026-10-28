#include "resource_network/cache.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
bool CacheCall(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  const auto method=CacheMethod(WebKit::Slot(args));
  const char* names[]={"match","matchAll","add","addAll","put","delete","keys"};
  const unsigned required[]={1,0,1,1,2,1,0};
  const char* name=names[int(method)];
  JS::RootedObject self(cx);
  const std::string message=std::string("'")+name+"' called on an object that does not implement interface Cache.";
  self=args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr;
  if (!self || JS::GetClass(self)!=&kCacheClass) {
    Error(cx,message.c_str());return CacheRejectPending(cx,args.rval());
  }
  if (argc<required[int(method)]) {
    std::string detail="At least "+std::to_string(required[int(method)])+" arguments required, but only "+std::to_string(argc)+" passed";
    if (required[int(method)]==1) detail="At least 1 argument required, but only 0 passed";
    CacheBindingError(cx,"Cache",name,detail.c_str());return CacheRejectPending(cx,args.rval());
  }
  State* state=CacheState(cx,self);
  if (!state) return false;
  state->Observe("Cache",name,int(ApiAccess::Call));
  if (method==CacheMethod::Add || method==CacheMethod::AddAll) {
    if (!CacheAddCall(*state,self,args,method==CacheMethod::AddAll)) return CacheRejectPending(cx,args.rval());
    return JS_WrapValue(cx,args.rval());
  }
  CacheStorageQueryDTO query;
  query.query.hasRequest=!args.get(0).isUndefined() || required[int(method)]!=0;
  bool request=false;std::string text;
  if (query.query.hasRequest && !CacheRequestInfo(*state,args.get(0),&query.query.request,&text,&request))
    return CacheRejectPending(cx,args.rval());
  if (method==CacheMethod::Put) {
    JS::RootedObject response(cx,BodyReceiver(args.get(1)));
    if (!response || JS::GetClass(response)!=&kResponseClass) {
      CacheBindingError(cx,"Cache","put","Argument 2 does not implement interface Response.");
      return CacheRejectPending(cx,args.rval());
    }
    if (!CacheResolveRequest(*state,&query.query.request,text,request,name) ||
        !CachePutCall(*state,self,args,std::move(query.query.request),response))
      return CacheRejectPending(cx,args.rval());
  } else {
    if (!CacheOptions(cx,args.get(1),&query,false,name) ||
        (query.query.hasRequest && !CacheResolveRequest(*state,&query.query.request,text,request,name)))
      return CacheRejectPending(cx,args.rval());
    query.query.kind=method==CacheMethod::Keys?CacheResultKind::Requests:
        method==CacheMethod::MatchAll?CacheResultKind::AllResponses:CacheResultKind::FirstResponse;
    if (!CacheQueryCall(*state,self,args,method,query.query)) return CacheRejectPending(cx,args.rval());
  }
  return JS_WrapValue(cx,args.rval());
}
}
