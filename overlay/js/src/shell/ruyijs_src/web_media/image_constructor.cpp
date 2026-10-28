#include "web_media/image_decoder.h"
#include "resource_network/readable_stream.h"
#include "runtime/native_constructor_trace.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/GlobalObject.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include <new>
namespace ruyijs::web_media {
namespace {
void Finalize(JS::GCContext*,JSObject* object) { delete ImageRecordOf(object); }
const JSClassOps kOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize};
bool ReadOptions(JSContext* cx,JS::HandleValue input,ImageRecord* record,
    JS::MutableHandleObject stream,JS::MutableHandleObjectVector transfers) {
  if(!input.isNullOrUndefined()&&!input.isObject())return WebKit::Fail(cx,"ImageDecoder constructor: Argument 1 can't be converted to a dictionary.");
  JS::RootedObject dictionary(cx,input.isObject()?&input.toObject():nullptr);
  JS::RootedValue value(cx),data(cx);bool hasWidth=false,hasHeight=false;
  for(const char* field:{"colorSpaceConversion","data","desiredHeight","desiredWidth","preferAnimation","transfer","type"}) {
    value.setUndefined();if(dictionary&&!JS_GetProperty(cx,dictionary,field,&value))return false;
    if(!strcmp(field,"colorSpaceConversion")) {
      if(value.isUndefined())continue;std::string conversion;if(!ImageString(cx,value,&conversion))return false;
      if(conversion!="default"&&conversion!="none")return WebKit::Fail(cx,"ImageDecoder constructor: Invalid colorSpaceConversion");
      record->noColorConversion=conversion=="none";
    } else if(!strcmp(field,"data")) {
      if(value.isUndefined())return WebKit::Fail(cx,"ImageDecoder constructor: Missing required 'data' member of ImageDecoderInit.");
      data.set(value);stream.set(resource_network::StreamReceiver(value));
      if(!stream&&!ImageBuffer(cx,value,&record->bytes))return false;
    } else if(!strcmp(field,"desiredHeight")) {
      hasHeight=!value.isUndefined();if(hasHeight&&!ImageUnsigned(cx,value,field,&record->height))return false;
    } else if(!strcmp(field,"desiredWidth")) {
      hasWidth=!value.isUndefined();if(hasWidth&&!ImageUnsigned(cx,value,field,&record->width))return false;
    } else if(!strcmp(field,"preferAnimation"))record->preferAnimation=value.isUndefined()||JS::ToBoolean(value);
    else if(!strcmp(field,"transfer")&&!value.isUndefined()) {
      JS::ForOfIterator iterator(cx);if(!iterator.init(value))return false;
      bool done=false;while(true) {
        JS::RootedValue item(cx);if(!iterator.next(&item,&done))return false;if(done)break;
        JS::RootedObject buffer(cx,item.isObject()?js::CheckedUnwrapStatic(&item.toObject()):nullptr);
        if(!buffer||!JS::IsArrayBufferObject(buffer))return WebKit::Fail(cx,"ImageDecoder constructor: transfer requires ArrayBuffer values");
        if(!transfers.append(buffer))return false;
      }
    } else if(!strcmp(field,"type")) {
      if(value.isUndefined())return WebKit::Fail(cx,"ImageDecoder constructor: Missing required 'type' member of ImageDecoderInit.");
      if(!ImageString(cx,value,&record->type))return false;
    }
  }
  if(record->type.rfind("image/",0)!=0)return WebKit::Fail(cx,"ImageDecoder constructor: Invalid MIME type, must be 'image'");
  if(stream&&(resource_network::SlotObject(stream,resource_network::SReader)||JS::GetReservedSlot(stream,resource_network::SDisturbed).toBoolean()))
    return WebKit::Fail(cx,"ImageDecoder constructor: ReadableStream data is disturbed and/or locked");
  if(!stream&&record->bytes.empty())return WebKit::Fail(cx,"ImageDecoder constructor: BufferSource is detached/empty");
  record->hasDesiredSize=hasWidth;record->inputComplete=!stream;
  return hasWidth==hasHeight || WebKit::Fail(cx,"ImageDecoder constructor: Both or neither of desiredHeight and desiredWidth must be passed");
}
}
const JSClass kImageDecoderClass={"ImageDecoder",JSCLASS_HAS_RESERVED_SLOTS(IDSlots)|JSCLASS_FOREGROUND_FINALIZE,&kOps};
bool ImageDecoderConstructor(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
  if(!args.isConstructing())return trace.Finish(WebKit::Fail(cx,"ImageDecoder constructor: 'new' is required"));
  JS::RootedObject prototype(cx),factories(cx),global(cx,JS::CurrentGlobalOrNull(cx)),stream(cx);
  JS::RootedObjectVector transfers(cx);
  if(!ImageDesiredPrototype(cx,args,IFDecoder,&prototype)||!ImageFactories(cx,global,&factories))return false;
  std::unique_ptr<ImageRecord> record(new(std::nothrow)ImageRecord);
  if(!record){JS_ReportOutOfMemory(cx);return false;}
  if(!ReadOptions(cx,args.get(0),record.get(),&stream,&transfers))return trace.Finish(false);
  for(size_t i=0;i<transfers.length();++i) {
    for(size_t j=0;j<i;++j)if(transfers[i]==transfers[j])return ImageException(cx,factories,"DataCloneError","Transfer contains duplicate ArrayBuffer objects");
    if(!JS::GetArrayBufferByteLength(transfers[i]))return ImageException(cx,factories,"DataCloneError","Transfer contains empty/detached ArrayBuffer objects");
  }
  JS::RootedObject object(cx,JS_NewObjectWithGivenProto(cx,&kImageDecoderClass,prototype)),tracks(cx);
  JS::RootedObject completed(cx,JS::NewPromiseObject(cx,nullptr));
  JS::RootedString type(cx,JS_NewStringCopyN(cx,record->type.data(),record->type.size()));
  if(!object||!completed||!type||!NewImageTracks(cx,factories,&tracks))return false;
  JS::SetReservedSlot(object,IDRecord,JS::PrivateValue(record.release()));
  JS::SetReservedSlot(object,IDFactories,JS::ObjectValue(*factories));
  JS::SetReservedSlot(object,IDType,JS::StringValue(type));JS::SetReservedSlot(object,IDComplete,JS::BooleanValue(false));
  JS::SetReservedSlot(object,IDCompleted,JS::ObjectValue(*completed));JS::SetReservedSlot(object,IDTracks,JS::ObjectValue(*tracks));
  for(unsigned slot:{IDHead,IDTail,IDReader,IDStreamFactories})JS::SetReservedSlot(object,slot,JS::NullValue());
  for(auto& entry:transfers) {JS::RootedObject buffer(cx,entry);if(!JS::DetachArrayBuffer(cx,buffer))return false;}
  if(!ImageCodecSupports(ImageRecordOf(object)->type)) {
    ImageRecordOf(object)->unsupported=true;
    if(!CloseImageDecoder(cx,object,"NotSupportedError","Unsupported mime type"))return false;
  } else if(stream) {if(!StartImageStream(cx,object,stream))return false;}
  else if(!ScheduleImageMetadata(cx,object))return false;
  args.rval().setObject(*object);return trace.Finish(true);
}
}
