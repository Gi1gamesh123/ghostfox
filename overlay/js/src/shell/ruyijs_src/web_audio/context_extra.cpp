#include "web_audio/node_fields.h"
#include "js/ArrayBuffer.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/ValueArray.h"

namespace ruyijs::web_audio {
bool MediaFactory(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  if(!detail::IsContextKind(cx,args.thisv(),false)) return false;
  if(WebKit::Slot(args)==3)
    return WebKit::Fail(cx,"createMediaStreamTrackSource requires a MediaStreamTrack");
  args.rval().setNull();return true;
}
bool DecodeAudioData(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  State* s=detail::Self(args);
  if(!detail::IsContext(cx,args.thisv())) return false;
  if(!args.get(0).isObject() || !JS::IsArrayBufferObject(&args[0].toObject()))
    return WebKit::Fail(cx,"decodeAudioData requires an ArrayBuffer");
  WebKit::FailKind(cx,"Audio decoding is not implemented",JSEXN_ERR);
  JS::RootedValue error(cx);
  if(!JS_GetPendingException(cx,&error)) return false;
  JS_ClearPendingException(cx);
  if(args.get(2).isObject() && JS::IsCallable(&args[2].toObject())) {
    JS::RootedValueArray<1> values(cx);values[0].set(error);
    JS::RootedValue ignored(cx);
    if(!JS::Call(cx,JS::UndefinedHandleValue,args[2],values,&ignored)) return false;
  }
  return s->kit.RejectWith(error,args.rval());
}
bool InstallContextSurface(State& s) {
  WebKit& kit=s.kit;
  if(!kit.Accessor(s.base,"BaseAudioContext","onstatechange",AudioEventGet,AudioEventSet,
                   kContextStateHandlerSlot) ||
     !kit.Method(s.base,"BaseAudioContext","decodeAudioData",DecodeAudioData,1,0) ||
     !kit.Accessor(s.context,"AudioContext","baseLatency",ContextGet,nullptr,6) ||
     !kit.Accessor(s.context,"AudioContext","outputLatency",ContextGet,nullptr,7) ||
     !kit.Method(s.base,"BaseAudioContext","resume",ContextPromise,0,0)) return false;
  const char* media[]={"createMediaElementSource","createMediaStreamSource",
                       "createMediaStreamDestination","createMediaStreamTrackSource"};
  for(int i=0;i<4;++i)
    if(!kit.Method(s.context,"AudioContext",media[i],MediaFactory,i==2?0:1,i)) return false;
  return kit.Method(s.context,"AudioContext","suspend",ContextPromise,0,1) &&
         kit.Method(s.context,"AudioContext","close",ContextPromise,0,2);
}
}
