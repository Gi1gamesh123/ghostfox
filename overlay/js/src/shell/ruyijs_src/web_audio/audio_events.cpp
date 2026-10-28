#include "web_audio/node_fields.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/ValueArray.h"

namespace ruyijs::web_audio {
namespace {
bool Delegate(State& s,JS::HandleObject self,const char* method,const char* type,
              JS::HandleValue callback) {
  JS::RootedObject proto(s.cx,s.events->prototype());
  JS::RootedValue fn(s.cx);
  JS::RootedString name(s.cx,JS_NewStringCopyZ(s.cx,type));
  if(!name || !JS_GetProperty(s.cx,proto,method,&fn)) return false;
  JS::RootedValueArray<2> arguments(s.cx);
  arguments[0].setString(name);arguments[1].set(callback);
  JS::RootedValue ignored(s.cx);
  return JS::Call(s.cx,self,fn,arguments,&ignored);
}
bool Receiver(JSContext* cx,const JS::CallArgs& args,int32_t slot,
               JS::MutableHandleObject object) {
  if(slot!=kNodeEndedSlot) {
    if(slot==kContextOnCompleteSlot ? !detail::IsContextKind(cx,args.thisv(),true)
                                    : !detail::IsContext(cx,args.thisv())) return false;
    object.set(&args.thisv().toObject());return true;
  }
  if(!detail::NodeReceiver(cx,args,kGenericNode,object)) return false;
  const auto def=JS::GetReservedSlot(object,kNodeDefIndexSlot).toInt32();
  if(JS::GetReservedSlot(object,kNodeKindSlot).toInt32()==kOscillatorNode ||
     def==13 || def==14) return true;
  return WebKit::Fail(cx,"AudioScheduledSourceNode requires a source receiver");
}
const char* EventName(int32_t slot) {
  if(slot==kNodeEndedSlot) return "ended";
  if(slot==kContextStateHandlerSlot) return "statechange";
  return "complete";
}
}
bool ReplaceHandler(State& s,JS::HandleObject self,const char* type,
                     JS::HandleValue old,JS::MutableHandleValue value) {
  if(old.isObject() && !Delegate(s,self,"removeEventListener",type,old)) return false;
  if(value.isObject() && JS::IsCallable(&value.toObject())) {
    if(!Delegate(s,self,"addEventListener",type,value)) return false;
  } else value.setNull();
  return true;
}
bool AudioEventGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  const int32_t slot=WebKit::Slot(args);
  JS::RootedObject object(cx);
  if(!Receiver(cx,args,slot,&object)) return false;
  JS::Value value=JS::GetReservedSlot(object,slot);
  args.rval().set(value.isUndefined()?JS::NullValue():value);return true;
}
bool AudioEventSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  State* s=detail::Self(args);
  const int32_t slot=WebKit::Slot(args);
  JS::RootedObject object(cx);
  if(!Receiver(cx,args,slot,&object)) return false;
  JS::RootedValue old(cx,JS::GetReservedSlot(object,slot)),value(cx,args.get(0));
  if(!ReplaceHandler(*s,object,EventName(slot),old,&value)) return false;
  JS::SetReservedSlot(object,slot,value);
  args.rval().setUndefined();return true;
}
}
