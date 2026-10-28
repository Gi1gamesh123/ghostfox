#include "web_audio/node_fields.h"
#include <cstring>
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs::web_audio {
bool FieldReceiver(JSContext* cx, const JS::CallArgs& args, int32_t def,
                    JS::MutableHandleObject self) {
  if (!detail::NodeReceiver(cx,args,kGenericNode,self)) return false;
  if (JS::GetReservedSlot(self,kNodeDefIndexSlot).toInt32()!=def)
    return WebKit::Fail(cx,"Audio node member has an incompatible receiver");
  return true;
}
bool ReadField(JSContext* cx, JS::HandleObject node, const char* name,
               JS::MutableHandleValue value) {
  const int32_t def=JS::GetReservedSlot(node,kNodeDefIndexSlot).toInt32();
  JS::RootedObject fields(cx,&JS::GetReservedSlot(node,kNodeFieldsSlot).toObject());
  for(size_t i=0;i<kNodeFieldCount;++i)
    if(kNodeFields[i].node==def && !std::strcmp(kNodeFields[i].name,name))
      return JS_GetElement(cx,fields,uint32_t(i),value);
  return WebKit::Fail(cx,"Unknown Web Audio node member");
}
bool InitNodeFields(State& s, JS::HandleObject node, JS::HandleValue options) {
  JSContext* cx=s.cx;
  JS::RootedObject fields(cx,JS::NewArrayObject(cx,0));
  if(!fields) return false;
  JS::SetReservedSlot(node,kNodeFieldsSlot,JS::ObjectValue(*fields));
  JS::SetReservedSlot(node,kNodeEndedSlot,JS::NullValue());
  if(!s.events->Attach(node)) return false;
  const int32_t def=JS::GetReservedSlot(node,kNodeDefIndexSlot).toInt32();
  for(size_t i=0;i<kNodeFieldCount;++i) {
    const auto& f=kNodeFields[i];
    if(f.node!=def) continue;
    JS::RootedValue value(cx,JS::NullValue());
    if(f.type==FT::Param) {
      double maximum=f.maximum;
      if(def==2 && !std::strcmp(f.name,"frequency")) {
        JSObject* context=detail::ContextOf(node);
        if(context) maximum=JS::GetReservedSlot(context,kContextRateSlot).toNumber()/2;
      }
      JS::RootedObject param(cx,NewParam(s,{f.initial,f.minimum,maximum,true}));
      if(!param) return false;
      value.setObject(*param);
    } else if(f.type==FT::Map) {
      JS::RootedObject map(cx,NewParamMap(s));
      if(!map) return false;
      value.setObject(*map);
    } else if(f.type==FT::String) {
      JS::RootedString text(cx,JS_NewStringCopyZ(cx,f.text));
      if(!text) return false;
      value.setString(text);
    } else if(f.type==FT::Port && s.channelPort) {
      // AudioWorkletNode.port from the MessagePort owner; the processor-side
      // peer is kept beside the fields, past the table, for the node's lifetime.
      JS::RootedObject first(cx), second(cx);
      if(!s.channelPort(s.channelContext,&first,&second)) return false;
      JS::RootedValue peer(cx,JS::ObjectValue(*second));
      if(!JS_WrapValue(cx,&peer) || !JS_SetElement(cx,fields,uint32_t(kNodeFieldCount+i),peer)) return false;
      value.setObject(*first);
      if(!JS_WrapValue(cx,&value)) return false;
    } else if(f.type==FT::Number) value.setNumber(f.initial);
    else if(f.type==FT::Boolean) value.setBoolean(f.initial!=0);
    if(!JS_SetElement(cx,fields,uint32_t(i),value)) return false;
  }
  if(!options.isObject()) return true;
  JS::RootedObject opts(cx,&options.toObject());
  for(size_t i=0;i<kNodeFieldCount;++i) {
    const auto& f=kNodeFields[i];
    if(f.node!=def || (!f.writable && f.type!=FT::Param)) continue;
    JS::RootedValue value(cx);
    if(!JS_GetProperty(cx,opts,f.name,&value)) return false;
    if(value.isUndefined()) continue;
    if(f.type==FT::Param) {
      JS::RootedValue param(cx);
      double number=0;
      if(!JS_GetElement(cx,fields,uint32_t(i),&param) || !JS::ToNumber(cx,value,&number))
        return false;
      const uint32_t id=uint32_t(JS::GetReservedSlot(&param.toObject(),0).toInt32());
      if(!s.model.set_param_value(id,number)) return false;
    } else if(!WriteField(s,node,int32_t(i),value)) return false;
  }
  return true;
}
bool NodeFieldGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  const auto index=WebKit::Slot(args);
  const auto& f=kNodeFields[index];
  JS::RootedObject self(cx);
  if(!FieldReceiver(cx,args,f.node,&self)) return false;
  JS::RootedObject fields(cx,&JS::GetReservedSlot(self,kNodeFieldsSlot).toObject());
  if(f.type==FT::Bins) {
    JS::RootedValue size(cx);
    if(!ReadField(cx,self,"fftSize",&size)) return false;
    args.rval().setNumber(size.toNumber()/2);
    return true;
  }
  return JS_GetElement(cx,fields,uint32_t(index),args.rval());
}
bool NodeFieldSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  State* state=detail::Self(args);
  const auto index=WebKit::Slot(args);
  JS::RootedObject self(cx);
  if(!FieldReceiver(cx,args,kNodeFields[index].node,&self) ||
     !WriteField(*state,self,index,args.get(0))) return false;
  args.rval().setUndefined();
  return true;
}
}
