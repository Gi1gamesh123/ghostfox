#include "resource_network/cache.h"
#include "resource_network/native_fetch.h"
#include "resource_network/request.h"
#include "resource_network/abort.h"
#include "js/ForOfIterator.h"
#include "js/Conversions.h"
#include "js/Promise.h"
#include "js/Realm.h"
#include <new>

namespace ruyijs::resource_network {
namespace {
enum BatchSlot { BCache,BPromise,BData,BRemaining,BTerminal,BSlots };
enum ItemSlot { IBatch,IIndex,ISlots };
void Finalize(JS::GCContext*,JSObject* object) {
  auto value=JS::GetReservedSlot(object,BData);
  if (!value.isUndefined()) delete static_cast<CachePairsDTO*>(value.toPrivate());
}
const JSClassOps kOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize};
const JSClass kBatch={"Resource Cache fetch batch",JSCLASS_HAS_RESERVED_SLOTS(BSlots)|JSCLASS_FOREGROUND_FINALIZE,&kOps};
const JSClass kItem={"Resource Cache fetch item",JSCLASS_HAS_RESERVED_SLOTS(ISlots)};
bool Complete(JSContext* cx,JS::HandleObject batch,bool success,JS::HandleValue reason) {
  JS::RootedObject promise(cx,SlotObject(batch,BPromise));
  JS::RootedValue error(cx,reason);
  JS::SetReservedSlot(batch,BTerminal,JS::BooleanValue(true));
  auto data=JS::GetReservedSlot(batch,BData);
  if (!data.isUndefined()) delete static_cast<CachePairsDTO*>(data.toPrivate());
  for (unsigned i=BCache;i<=BData;++i) JS::SetReservedSlot(batch,i,JS::UndefinedValue());
  if (success) return JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue);
  if (!JS_WrapValue(cx,&error)) return false;
  return JS::RejectPromise(cx,promise,error);
}
bool FailPending(JSContext* cx,JS::HandleObject batch) {
  JS::RootedValue error(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&error)) return false;
  JS_ClearPendingException(cx);return Complete(cx,batch,false,error);
}
bool Finish(JSContext* cx,JS::HandleObject item,NativeFetchOutcome outcome,
              JS::HandleObject response,JS::HandleValue reason) {
  JS::RootedObject batch(cx,SlotObject(item,IBatch));
  JS::SetReservedSlot(item,IBatch,JS::UndefinedValue());
  JSAutoRealm realm(cx,batch);
  if (JS::GetReservedSlot(batch,BTerminal).isTrue()) return true;
  JS::RootedObject self(cx,SlotObject(batch,BCache));
  State* state=CacheState(cx,self);
  if (!state) return false;
  if (outcome!=NativeFetchOutcome::Complete) {
    if (outcome==NativeFetchOutcome::Aborted || outcome==NativeFetchOutcome::NativeFailure)
      return Complete(cx,batch,false,reason);
    Error(cx,"NetworkError when attempting to fetch resource.");return FailPending(cx,batch);
  }
  auto* values=static_cast<CachePairsDTO*>(JS::GetReservedSlot(batch,BData).toPrivate());
  const size_t index=size_t(JS::GetReservedSlot(item,IIndex).toInt32());
  auto& value=(*values)[index];
  if (!CacheResponseRead(*state,response,&value.response)) return FailPending(cx,batch);
  const auto& r=value.response;
  if (r.status<200 || r.status>299 || r.status==206 || r.type=="opaque" || r.type=="opaqueredirect") {
    const std::string message="Cache got "+r.type+" response with bad status "+
        std::to_string(r.status)+" while trying to add request "+value.request.url;
    Error(cx,message.c_str());return FailPending(cx,batch);
  }
  if (!CacheValidateResponse(cx,r,"addAll")) return FailPending(cx,batch);
  JS::RootedValue responseValue(cx,JS::ObjectValue(*response));
  const BodyRecord* body=BodyBytesOf(BodyHolderOf(responseValue));
  if (!body) {Error(cx,"Native fetch response has no buffered Body");return FailPending(cx,batch);}
  value.response.body=body->bytes;
  int remaining=JS::GetReservedSlot(batch,BRemaining).toInt32()-1;
  JS::SetReservedSlot(batch,BRemaining,JS::Int32Value(remaining));
  if (remaining) return true;
  const auto status=CachePutBatch(CacheNative(self),*values);
  if (status.code==ErrorCode::InvalidState) {
    JS::RootedValue error(cx);
    return AbortException(*state,"InvalidStateError",&error,
        "An attempt was made to use an object that is not, or is no longer, usable") && Complete(cx,batch,false,error);
  }
  if (!CacheReport(cx,status)) return FailPending(cx,batch);
  return Complete(cx,batch,true,JS::UndefinedHandleValue);
}
const NativeFetchSteps kFinish={Finish};
}
bool CacheAddCall(State& state,JS::HandleObject self,const JS::CallArgs& args,bool all) {
  JSContext* cx=state.cx;
  JS::RootedValueVector inputs(cx);
  if (all) {
    if (args.get(0).isNullOrUndefined()) return CacheBindingError(cx,"Cache","addAll",
        "Argument 1 can't be converted to a sequence.");
    JS::ForOfIterator iterator(cx);
    if (!iterator.init(args.get(0))) return false;
    JS::RootedValue item(cx);bool done=false;
    while (true) {
      if (!iterator.next(&item,&done)) return false;
      if (done) break;
      if (inputs.length()>=256) return Error(cx,"Cache addAll batch limit exceeded");
      // Convert the union now, before requesting the next iterator element.
      JS::RootedObject request(cx,BodyReceiver(item));
      if (!request || JS::GetClass(request)!=&kRequestClass) {
        JS::RootedString string(cx,JS::ToString(cx,item));
        if (!string) return false;
        item.setString(string);
      }
      if (!inputs.append(item)) return false;
    }
  } else if (!inputs.append(args.get(0))) return false;
  std::unique_ptr<CachePairsDTO> values(new(std::nothrow) CachePairsDTO(inputs.length()));
  if (!values) {JS_ReportOutOfMemory(cx);return false;}
  JS::RootedValueVector signals(cx);
  for (size_t i=0;i<inputs.length();++i) {
    JS::RootedValue signal(cx);
    JS::RootedObject existing(cx,BodyReceiver(inputs[i]));
    bool ok=existing && JS::GetClass(existing)==&kRequestClass?
        ReadRequest(state,existing,&(*values)[i].request,&signal):
        RequestInput(state,inputs[i],JS::UndefinedHandleValue,&(*values)[i].request,&signal);
    if (!ok ||
        !CacheValidateRequest(cx,(*values)[i].request,all?"addAll":"add") || !signals.append(signal)) return false;
  }
  JSAutoRealm realm(cx,self);
  JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr)),batch(cx);
  if (!promise) return false;
  args.rval().setObject(*promise);
  if (values->empty()) return JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue);
  batch=JS_NewObjectWithGivenProto(cx,&kBatch,nullptr);
  if (!batch) return false;
  JS::SetReservedSlot(batch,BCache,JS::ObjectValue(*self));
  JS::SetReservedSlot(batch,BPromise,JS::ObjectValue(*promise));
  JS::SetReservedSlot(batch,BRemaining,JS::Int32Value(int(values->size())));
  JS::SetReservedSlot(batch,BTerminal,JS::BooleanValue(false));
  auto* pairs=values.release();
  JS::SetReservedSlot(batch,BData,JS::PrivateValue(pairs));
  for (size_t i=0;i<pairs->size();++i) {
    JS::RootedObject item(cx,JS_NewObjectWithGivenProto(cx,&kItem,nullptr));
    if (!item) return FailPending(cx,batch);
    JS::SetReservedSlot(item,IBatch,JS::ObjectValue(*batch));
    JS::SetReservedSlot(item,IIndex,JS::Int32Value(int(i)));
    JS::RootedValue signal(cx,signals[i]);
    if (!JS_WrapValue(cx,&signal) || !FetchNative(state,(*pairs)[i].request,signal,item,kFinish))
      return FailPending(cx,batch);
    if (JS::GetReservedSlot(batch,BTerminal).isTrue()) break;
  }
  return true;
}
}
