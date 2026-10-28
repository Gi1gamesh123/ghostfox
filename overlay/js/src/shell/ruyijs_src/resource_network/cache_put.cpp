#include "resource_network/cache.h"
#include "resource_network/body_acquire.h"
#include "resource_network/abort.h"
#include "js/Promise.h"
#include "js/Realm.h"
#include <new>

namespace ruyijs::resource_network {
namespace {
enum PutSlot { PCache,PPromise,PPair,PSlots };
void Finalize(JS::GCContext*,JSObject* self) {
  auto value=JS::GetReservedSlot(self,PPair);
  if (!value.isUndefined()) delete static_cast<CachePairsDTO*>(value.toPrivate());
}
const JSClassOps kOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize};
const JSClass kPut={"Resource Cache put",JSCLASS_HAS_RESERVED_SLOTS(PSlots)|JSCLASS_FOREGROUND_FINALIZE,&kOps};
bool Finish(JSContext* cx,JS::HandleObject input,JS::HandleObject holder,
              BodyOutcome outcome,JS::HandleValue reason) {
  JS::RootedObject target(cx,js::CheckedUnwrapStatic(input));
  if (!target || JS::GetClass(target)!=&kPut) return Error(cx,"Invalid Cache put completion target");
  JSAutoRealm realm(cx,target);
  JS::RootedObject promise(cx,SlotObject(target,PPromise)),cache(cx,SlotObject(target,PCache));
  State* state=CacheState(cx,cache);
  if (!state) return false;
  auto* pairs=static_cast<CachePairsDTO*>(JS::GetReservedSlot(target,PPair).toPrivate());
  std::unique_ptr<CachePairsDTO> release(pairs);
  JS::SetReservedSlot(target,PPair,JS::UndefinedValue());
  JS::SetReservedSlot(target,PCache,JS::UndefinedValue());
  JS::SetReservedSlot(target,PPromise,JS::UndefinedValue());
  if (outcome==BodyOutcome::Complete) {
    const auto* bytes=BodyBytesOf(holder);
    pairs->front().response.body=bytes->bytes;
    if (!CacheReport(cx,CachePutBatch(CacheNative(cache),*pairs))) return CacheRejectPromise(cx,promise);
    return JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue);
  }
  if (outcome==BodyOutcome::Locked) {
    CacheBindingError(cx,"Cache","put","Cannot get a new reader for a readable stream already locked by another reader.");
    return CacheRejectPromise(cx,promise);
  }
  JS::RootedValue error(cx,reason);
  if (outcome!=BodyOutcome::NativeFailure &&
      !AbortException(*state,outcome==BodyOutcome::Retired?"AbortError":"NS_ERROR_FAILURE",&error,"")) return false;
  return JS_WrapValue(cx,&error) && JS::RejectPromise(cx,promise,error);
}
const BodyAcquireSteps kFinish={Finish};
}
bool CachePutCall(State& state,JS::HandleObject self,const JS::CallArgs& args,
                  RequestDraft request,JS::HandleObject response) {
  JSContext* cx=state.cx;
  if (!CacheValidateRequest(cx,request,"put")) return false;
  std::unique_ptr<CachePairsDTO> pairs(new(std::nothrow) CachePairsDTO(1));
  if (!pairs) {JS_ReportOutOfMemory(cx);return false;}
  pairs->front().request=std::move(request);
  if (!CacheResponseRead(state,response,&pairs->front().response) ||
      !CacheValidateResponse(cx,pairs->front().response,"put")) return false;
  JS::RootedValue input(cx,JS::ObjectValue(*response));
  JS::RootedObject holder(cx,BodyHolderOf(input));
  if (BodyUsed(holder)) return CacheBindingError(cx,"Cache","put","Body has already been consumed.");
  JSAutoRealm realm(cx,self);
  JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr));
  JS::RootedObject target(cx,JS_NewObjectWithGivenProto(cx,&kPut,nullptr));
  if (!promise || !target) return false;
  JS::SetReservedSlot(target,PCache,JS::ObjectValue(*self));
  JS::SetReservedSlot(target,PPromise,JS::ObjectValue(*promise));
  JS::SetReservedSlot(target,PPair,JS::PrivateValue(pairs.release()));
  if (!AcquireBody(cx,holder,BodyAcquireMode::AfterValidation,target,kFinish)) return false;
  args.rval().setObject(*promise);return true;
}
}
