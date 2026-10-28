// Response clone shares its original factory and the intrinsic original tee.
#include "resource_network/body_acquire.h"
#include "resource_network/readable_tee.h"
#include "resource_network/model_body_bytes.h"
#include "resource_network/state.h"
#include "js/Realm.h"
#include "js/String.h"
#include <memory>
#include <new>

namespace ruyijs::resource_network {
bool ResponseClone(JSContext* cx,const JS::CallArgs& args) {
  State& methodOwner=*FunctionOwner(args);
  JS::RootedObject self(cx,BodyReceiver(args.thisv()));
  if (!self || JS::GetClass(self)!=&kResponseClass) return BodyOperationError(cx,"Response","clone",true);
  methodOwner.Observe("Response","clone",int(ApiAccess::Call));
  JS::RootedObject holder(cx,SlotObject(self,0));
  if (BodyUsed(holder) || BodyLocked(holder)) return BodyOperationError(cx,"Response","clone",false);
  JS::RootedObject result(cx);
  {
    JSAutoRealm realm(cx,self);
    JS::RootedObject factories(cx,SlotObject(holder,HFactories));
    State* state=StreamFactoryState(cx,factories);
    if (!state) return false;
    BodyRecord* body=HolderBody(holder);
    std::unique_ptr<BodyRecord> copy(new(std::nothrow) BodyRecord());
    if (!copy) {JS_ReportOutOfMemory(cx);return false;}
    copy->nullBody=body->nullBody;copy->kind=body->kind;
    if (body->source!=BodySource::ScriptStream &&
        !Report(cx,CopyBodyBytes(BodyBytesOf(holder)->bytes,&copy->bytes))) return false;
    JS::RootedObject headers(cx,SlotObject(self,1)),clonedHeaders(cx);
    if (!CopyBodyHeaders(*state,headers,&clonedHeaders)) return false;
    JS::RootedString text(cx,JS::GetReservedSlot(self,3).toString()),url(cx,JS::GetReservedSlot(self,4).toString());
    JS::RootedString type(cx,JS::GetReservedSlot(self,5).toString());
    JS::UniqueChars encoded(JS_EncodeStringToUTF8(cx,type));
    if (!encoded || !ResponseNew(*state,state->responsePrototype,copy.get(),clonedHeaders,
        JS::GetReservedSlot(self,2).toInt32(),text,url,encoded.get(),JS::GetReservedSlot(self,6).toBoolean(),&result)) return false;
    copy.release();
    if (body->source==BodySource::ScriptStream) {
      JS::RootedObject source(cx,SlotObject(holder,HStream)),first(cx),second(cx),operation(cx),other(cx,SlotObject(result,0));
      if (!TeeReadable(cx,source,&first,&second,&operation)) return false;
      if (!JS_WrapObject(cx,&first) || !JS_WrapObject(cx,&second)) return TeeOperationFailure(cx,operation);
      JS::SetReservedSlot(holder,HStream,JS::ObjectValue(*first));
      JS::SetReservedSlot(other,HStream,JS::ObjectValue(*second));
      HolderBody(other)->source=BodySource::ScriptStream;
    }
  }
  args.rval().setObject(*result);return JS_WrapValue(cx,args.rval());
}
}
