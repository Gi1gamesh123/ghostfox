#include "web_media/image_decoder.h"
#include "web_media/image_progress.h"
#include "jsfriendapi.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
namespace ruyijs::web_media {
bool ProcessImageDecodes(JSContext* cx,JS::HandleObject decoder) {
  auto* record=ImageRecordOf(decoder);
  if(record->closed||!record->established)return true;
  JS::RootedObject factories(cx,ImageObject(decoder,IDFactories));
  JS::RootedObject request(cx,ImageObject(decoder,IDHead)),previous(cx);
  while(request) {
    JS::RootedObject promise(cx,ImageObject(request,DRPromise)),next(cx,ImageObject(request,DRNext));
    const uint32_t index=uint32_t(JS::GetReservedSlot(request,DRIndex).toNumber());
    if(index>=record->image.frames.size()&&!record->inputComplete&&
        (!record->frameCountFinal||index<record->frameCount)) {
      previous=request;request=next;continue;
    }
    if(previous)JS::SetReservedSlot(previous,DRNext,JS::GetReservedSlot(request,DRNext));
    else JS::SetReservedSlot(decoder,IDHead,JS::GetReservedSlot(request,DRNext));
    if(!next)JS::SetReservedSlot(decoder,IDTail,previous?JS::ObjectValue(*previous):JS::NullValue());
    request=next;
    if(record->hasDesiredSize&&(!record->width||!record->height)) {
      ImageException(cx,factories,"InvalidStateError","Error decoding frame");
      if(!ImageReject(cx,promise))return false;continue;
    }
    if(index>=record->image.frames.size()) {
      WebKit::FailKind(cx,"No more frames available",JSEXN_RANGEERR);
      if(!ImageReject(cx,promise))return false;continue;
    }
    JS::RootedObject frame(cx),result(cx,JS_NewPlainObject(cx));
    if(!result||!NewVideoFrame(cx,factories,record->image.frames[index],&frame))return false;
    JS::RootedValue value(cx,JS::ObjectValue(*frame));
    if(!JS_DefineProperty(cx,result,"image",value,JSPROP_ENUMERATE))return false;
    value.setBoolean(true);if(!JS_DefineProperty(cx,result,"complete",value,JSPROP_ENUMERATE))return false;
    value.setObject(*result);if(!JS::ResolvePromise(cx,promise,value))return false;
  }
  return true;
}
namespace {
bool MetadataTask(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject decoder(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  args.rval().setUndefined();
  JSAutoRealm realm(cx,decoder);auto* record=ImageRecordOf(decoder);
  if(record->closed)return true;
  if(record->processedBytes!=record->bytes.size()||record->processedEof!=record->inputComplete) {
    ImageProgress progress;
    const auto status=DecodeImageProgress(record->type,record->bytes,record->width,record->height,
        record->noColorConversion,record->inputComplete,&progress);
    record->processedBytes=record->bytes.size();record->processedEof=record->inputComplete;
    if(status==domain::web_media::ImageDecodeStatus::OutOfMemory){JS_ReportOutOfMemory(cx);return false;}
    if(status!=domain::web_media::ImageDecodeStatus::Ok)
      return CloseImageDecoder(cx,decoder,"EncodingError","Metadata decoding failed");
    if(progress.metadata) {
      record->image=std::move(progress.image);record->frameCount=progress.frameCount;record->frameCountFinal=progress.frameCountFinal;
      if(!record->established){record->established=true;if(!EstablishImageTracks(cx,decoder))return false;}
      else {
        JS::RootedObject list(cx,ImageObject(decoder,IDTracks)),track(cx,ImageObject(list,ILTrack));
        if(track){JS::SetReservedSlot(track,ITCount,JS::NumberValue(record->frameCount));
          JS::SetReservedSlot(track,ITAnimated,JS::BooleanValue(record->image.animated));
          JS::SetReservedSlot(track,ITRepeat,JS::NumberValue(record->image.repetitions));}
      }
    }
    if(record->inputComplete) {
      JS::SetReservedSlot(decoder,IDComplete,JS::BooleanValue(true));
      JS::RootedObject completed(cx,ImageObject(decoder,IDCompleted));
      if(!JS::ResolvePromise(cx,completed,JS::UndefinedHandleValue))return false;
    }
  }
  return ProcessImageDecodes(cx,decoder);
}
}
bool ScheduleImageMetadata(JSContext* cx,JS::HandleObject decoder) {
  JSAutoRealm realm(cx,decoder);
  JS::RootedFunction function(cx,js::NewFunctionWithReserved(cx,MetadataTask,0,0,""));
  if(!function)return false;
  JS::RootedObject callback(cx,JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(callback,0,JS::ObjectValue(*decoder));
  JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr));
  return promise && JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue) &&
      JS::AddPromiseReactions(cx,promise,callback,nullptr);
}
}
