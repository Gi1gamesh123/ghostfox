#include "web_media/image_decoder.h"
#include "web_media/media_runtime.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/GlobalObject.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
namespace ruyijs::web_media {
namespace {
const JSClass kRequestClass={"ImageDecodeRequest",JSCLASS_HAS_RESERVED_SLOTS(DRSlots)};
}
bool ResetImageDecoder(JSContext* cx,JS::HandleObject object,const char* name,const char* message) {
  JS::RootedObject factories(cx,ImageObject(object,IDFactories));
  JS::RootedObject request(cx,ImageObject(object,IDHead));
  JS::SetReservedSlot(object,IDHead,JS::NullValue());JS::SetReservedSlot(object,IDTail,JS::NullValue());
  while(request) {
    JS::RootedObject promise(cx,ImageObject(request,DRPromise)),next(cx,ImageObject(request,DRNext));
    ImageException(cx,factories,name,message);if(!ImageReject(cx,promise))return false;request=next;
  }
  return true;
}
bool CloseImageDecoder(JSContext* cx,JS::HandleObject object,const char* name,const char* message) {
  auto* record=ImageRecordOf(object);if(record->closed)return true;record->closed=true;
  if(!ResetImageDecoder(cx,object,name,message))return false;
  JS::RootedObject factories(cx,ImageObject(object,IDFactories)),list(cx,ImageObject(object,IDTracks));
  JS::RootedObject ready(cx,ImageObject(list,ILReady)),completed(cx,ImageObject(object,IDCompleted));
  if(JS::GetPromiseState(ready)==JS::PromiseState::Pending){ImageException(cx,factories,name,message);if(!ImageReject(cx,ready))return false;}
  if(JS::GetPromiseState(completed)==JS::PromiseState::Pending){ImageException(cx,factories,name,message);if(!ImageReject(cx,completed))return false;}
  JS::SetReservedSlot(object,IDComplete,JS::BooleanValue(true));
  JS::RootedString empty(cx,JS_GetEmptyString(cx));if(!empty)return false;
  JS::SetReservedSlot(object,IDType,JS::StringValue(empty));
  JS::RootedObject track(cx,ImageObject(list,ILTrack));
  if(track)JS::SetReservedSlot(track,ITDecoder,JS::NullValue());
  js::SetProxyReservedSlot(list,ILTrack,JS::NullValue());
  std::vector<uint8_t>().swap(record->bytes);domain::web_media::ImageData().frames.swap(record->image.frames);
  return CancelImageStream(cx,object);
}
bool ImageDecoderGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);const unsigned field=unsigned(WebKit::Slot(args));
  static const char* names[]={"get type","get complete","get completed","get tracks"};
  static const unsigned slots[]={IDType,IDComplete,IDCompleted,IDTracks};
  JS::RootedObject self(cx);if(!ImageBrand(cx,args.thisv(),&kImageDecoderClass,names[field],&self))return false;
  args.rval().set(JS::GetReservedSlot(self,slots[field]));return JS_WrapValue(cx,args.rval());
}
bool ImageDecoderMethod(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);const unsigned method=unsigned(WebKit::Slot(args));
  static const char* names[]={"decode","reset","close"};
  JS::RootedObject self(cx),caller(cx,JS::CurrentGlobalOrNull(cx));
  if(!ImageBrand(cx,args.thisv(),&kImageDecoderClass,names[method],&self))
    return method?false:RejectMediaException(cx,caller,args.rval());
  if(method) {
    JSAutoRealm realm(cx,self);args.rval().setUndefined();
    return method==1?ResetImageDecoder(cx,self,"AbortError","Reset decoder"):CloseImageDecoder(cx,self,"AbortError","Closed decoder");
  }
  uint32_t index=0;JS::RootedValue value(cx);JS::RootedObject options(cx);
  if(args.get(0).isObject())options=&args[0].toObject();
  else if(!args.get(0).isNullOrUndefined()){
    WebKit::Fail(cx,"ImageDecoder.decode: Argument 1 can't be converted to a dictionary.");return RejectMediaException(cx,caller,args.rval());
  }
  if(options&&(!JS_GetProperty(cx,options,"completeFramesOnly",&value)||!JS_GetProperty(cx,options,"frameIndex",&value)||
       (!value.isUndefined()&&!ImageUnsigned(cx,value,"frameIndex",&index))))return RejectMediaException(cx,caller,args.rval());
  JS::RootedObject promise(cx);
  {
    JSAutoRealm realm(cx,self);promise=JS::NewPromiseObject(cx,nullptr);if(!promise)return false;
    JS::RootedObject factories(cx,ImageObject(self,IDFactories)),list(cx,ImageObject(self,IDTracks)),track(cx,ImageObject(list,ILTrack));
    auto* record=ImageRecordOf(self);
    if(record->unsupported||record->closed||(record->established&&(!track||!JS::GetReservedSlot(track,ITSelected).toBoolean()))) {
      ImageException(cx,factories,record->unsupported?"NotSupportedError":"InvalidStateError",
        record->unsupported?"Unsupported MIME type":record->closed?"Closed decoder":"No track selected");
      if(!ImageReject(cx,promise))return false;
    } else {
      JS::RootedObject request(cx,JS_NewObject(cx,&kRequestClass));if(!request)return false;
      JS::SetReservedSlot(request,DRPromise,JS::ObjectValue(*promise));JS::SetReservedSlot(request,DRIndex,JS::NumberValue(index));
      JS::SetReservedSlot(request,DRNext,JS::NullValue());
      JSObject* tail=ImageObject(self,IDTail);if(tail)JS::SetReservedSlot(tail,DRNext,JS::ObjectValue(*request));
      else JS::SetReservedSlot(self,IDHead,JS::ObjectValue(*request));JS::SetReservedSlot(self,IDTail,JS::ObjectValue(*request));
      if(!ScheduleImageMetadata(cx,self))return false;
    }
  }
  if(!JS_WrapObject(cx,&promise))return false;args.rval().setObject(*promise);return true;
}
bool ImageTypeSupported(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject caller(cx,JS::CurrentGlobalOrNull(cx));std::string type;
  if(!argc){WebKit::Fail(cx,"ImageDecoder.isTypeSupported: At least 1 argument required, but only 0 passed");return RejectMediaException(cx,caller,args.rval());}
  if(!ImageString(cx,args[0],&type))return RejectMediaException(cx,caller,args.rval());
  if(type.rfind("image/",0)!=0){WebKit::Fail(cx,"Invalid MIME type, must be 'image'");return RejectMediaException(cx,caller,args.rval());}
  JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr));JS::RootedValue result(cx,JS::BooleanValue(ImageCodecSupports(type)));
  if(!promise||!JS::ResolvePromise(cx,promise,result))return false;args.rval().setObject(*promise);return true;
}
}
