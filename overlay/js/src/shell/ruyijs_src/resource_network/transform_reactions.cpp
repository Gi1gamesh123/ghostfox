// Real intrinsic result Promises preserve pressure versus direct-entry behavior.
#include "resource_network/transform_internal.h"
#include "resource_network/state.h"
#include "jsfriendapi.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
enum ReactionSlot { RController,RKind,RChunk,RSlots };
const JSClass kReaction={"Resource transform reaction",JSCLASS_HAS_RESERVED_SLOTS(RSlots)};
bool Throw(JSContext* cx,JS::HandleValue reason) {
  JS::RootedValue value(cx,reason);
  if (!JS_WrapValue(cx,&value)) return false;
  JS_SetPendingException(cx,value);return false;
}
bool React(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject record(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  JS::RootedObject controller(cx,SlotObject(record,RController)),stream(cx,SlotObject(controller,CTransform));
  const auto kind=TransformReaction(JS::GetReservedSlot(record,RKind).toInt32());
  const bool reject=js::GetFunctionNativeReserved(&args.callee(),1).toBoolean();
  JSAutoRealm realm(cx,controller);JS::RootedValue value(cx,args.get(0));
  if (!JS_WrapValue(cx,&value)) return TransformNativeFailure(cx,stream);
  args.rval().setUndefined();
  if (kind==TransformReaction::Pressure) {
    JS::RootedObject writable(cx,SlotObject(stream,TWritable));
    if (WritableStatus(writable)!=WriteStatus::Writable) {
      JS::RootedValue reason(cx,JS::GetReservedSlot(writable,WError));return Throw(cx,reason);
    }
    JS::RootedValue chunk(cx,JS::GetReservedSlot(record,RChunk));
    const auto outcome=PerformTransform(cx,controller,chunk,args.rval(),{});
    if (outcome==CallbackResult::EntryRejected) return false;
    return outcome==CallbackResult::Success || TransformNativeFailure(cx,stream);
  }
  JS::RootedObject readable(cx,SlotObject(stream,TReadable)),source(cx,SlotObject(readable,SController));
  if (reject) {
    if (!TransformError(cx,stream,value)) return TransformNativeFailure(cx,stream);
    if (kind==TransformReaction::Flush) value=JS::GetReservedSlot(readable,SError);
    return Throw(cx,value);
  }
  if (kind==TransformReaction::Flush) {
    if (ReadableStatus(readable)==StreamStatus::Errored) {
      JS::RootedValue reason(cx,JS::GetReservedSlot(readable,SError));return Throw(cx,reason);
    }
    if (ReadableStatus(readable)==StreamStatus::Readable && !(DefaultFlags(source)&DCloseRequested) &&
        !DefaultClose(cx,source)) return TransformNativeFailure(cx,stream);
  }
  return true;
}
}
bool TransformThen(JSContext* cx,JS::HandleObject controller,JS::HandleObject promise,
    TransformReaction kind,JS::HandleValue chunk,JS::MutableHandleValue result) {
  JSAutoRealm realm(cx,controller);
  JS::RootedObject record(cx,JS_NewObjectWithGivenProto(cx,&kReaction,nullptr)),local(cx,promise);
  JS::RootedObject fulfilled(cx),rejected(cx);JS::RootedValue input(cx,chunk);
  if (!record || !JS_WrapObject(cx,&local) || !JS_WrapValue(cx,&input)) return false;
  JS::SetReservedSlot(record,RController,JS::ObjectValue(*controller));
  JS::SetReservedSlot(record,RKind,JS::Int32Value(int(kind)));JS::SetReservedSlot(record,RChunk,input);
  for (unsigned reject=0;reject<2;++reject) {
    if ((kind==TransformReaction::Pressure && reject) || (kind==TransformReaction::Transform && !reject)) continue;
    JS::RootedFunction fn(cx,js::NewFunctionWithReserved(cx,React,1,0,nullptr));if (!fn) return false;
    JS::RootedObject function(cx,JS_GetFunctionObject(fn));
    js::SetFunctionNativeReserved(function,0,JS::ObjectValue(*record));
    js::SetFunctionNativeReserved(function,1,JS::BooleanValue(reject));
    if (reject) rejected=function;else fulfilled=function;
  }
  JS::RootedObject next(cx,JS::CallOriginalPromiseThen(cx,local,fulfilled,rejected));
  if (!next) return false;
  result.setObject(*next);return true;
}
}
