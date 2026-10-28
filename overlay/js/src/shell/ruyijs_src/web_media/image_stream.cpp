// Encoded stream input uses ResourceNetwork's original native read completion.
#include "web_media/image_decoder.h"
#include "resource_network/readable_stream.h"
#include "jsfriendapi.h"
#include "js/Promise.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
namespace ruyijs::web_media {
namespace {
bool Read(JSContext*,JS::HandleObject);
bool QueueRead(JSContext* cx,JS::HandleObject decoder);
bool Chunk(JSContext* cx,JS::HandleObject,JS::HandleObject decoder,JS::HandleValue value) {
  if(ImageRecordOf(decoder)->closed)return true;
  JS::RootedObject chunk(cx,value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
  if(!chunk||!JS_IsUint8Array(chunk))return CloseImageDecoder(cx,decoder,"EncodingError","Invalid image stream chunk");
  bool ok;
  {JS::AutoCheckCannotGC nogc;bool shared=false;
    const auto* bytes=static_cast<const uint8_t*>(JS_GetArrayBufferViewData(chunk,&shared,nogc));
    ok=ImageAppendBytes(&ImageRecordOf(decoder)->bytes,bytes,JS_GetArrayBufferViewByteLength(chunk));}
  if(!ok){JS_ReportOutOfMemory(cx);return false;}
  return QueueRead(cx,decoder)&&ScheduleImageMetadata(cx,decoder);
}
bool End(JSContext* cx,JS::HandleObject,JS::HandleObject decoder,JS::HandleValue) {
  if(ImageRecordOf(decoder)->closed)return true;
  ImageRecordOf(decoder)->inputComplete=true;
  return ScheduleImageMetadata(cx,decoder);
}
bool Failed(JSContext* cx,JS::HandleObject,JS::HandleObject decoder,JS::HandleValue) {
  return CloseImageDecoder(cx,decoder,"EncodingError","Image stream failed");
}
const resource_network::ReadCompletionSteps kRead={Chunk,End,Failed};
bool Read(JSContext* cx,JS::HandleObject decoder) {
  if(ImageRecordOf(decoder)->closed||ImageRecordOf(decoder)->inputComplete)return true;
  JS::RootedObject reader(cx,ImageObject(decoder,IDReader)),factories(cx,ImageObject(decoder,IDStreamFactories)),target(cx);
  reader=js::CheckedUnwrapStatic(reader);
  auto* state=resource_network::StreamFactoryState(cx,factories);
  if(!state)return false;
  return resource_network::NewReadCompletion(cx,decoder,decoder,kRead,&target) &&
      resource_network::ReadDefaultReaderInto(*state,reader,target);
}
bool ReadTask(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject decoder(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  args.rval().setUndefined();
  JSAutoRealm realm(cx,decoder);return Read(cx,decoder);
}
bool QueueRead(JSContext* cx,JS::HandleObject decoder) {
  JS::RootedFunction function(cx,js::NewFunctionWithReserved(cx,ReadTask,0,0,""));
  if(!function)return false;JS::RootedObject callback(cx,JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(callback,0,JS::ObjectValue(*decoder));
  JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr));
  return promise&&JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue)&&JS::AddPromiseReactions(cx,promise,callback,nullptr);
}
}
bool StartImageStream(JSContext* cx,JS::HandleObject decoder,JS::HandleObject stream) {
  JS::RootedObject reader(cx),factories(cx);
  if(!resource_network::StreamFactories(cx,stream,&factories))return false;
  JS::RootedObject prototype(cx,resource_network::SlotObject(stream,resource_network::SReaderPrototype));
  if(!resource_network::NewDefaultReader(cx,stream,prototype,&reader))return false;
  if(!JS_WrapObject(cx,&reader)||!JS_WrapObject(cx,&factories))return false;
  JS::SetReservedSlot(decoder,IDReader,JS::ObjectValue(*reader));
  JS::SetReservedSlot(decoder,IDStreamFactories,JS::ObjectValue(*factories));
  return ScheduleImageMetadata(cx,decoder)&&QueueRead(cx,decoder);
}
bool CancelImageStream(JSContext* cx,JS::HandleObject decoder) {
  JS::RootedObject reader(cx,ImageObject(decoder,IDReader));if(!reader)return true;
  reader=js::CheckedUnwrapStatic(reader);
  JS::SetReservedSlot(decoder,IDReader,JS::NullValue());JS::SetReservedSlot(decoder,IDStreamFactories,JS::NullValue());
  JS::RootedValue result(cx);
  if(!resource_network::CancelReader(cx,reader,JS::UndefinedHandleValue,&result))return false;
  if(result.isObject()){JS::RootedObject promise(cx,&result.toObject());if(JS::IsPromiseObject(promise)&&!JS::SetAnyPromiseIsHandled(cx,promise))return false;}
  return true;
}
}
