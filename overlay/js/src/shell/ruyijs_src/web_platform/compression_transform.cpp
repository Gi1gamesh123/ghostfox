// Original per-call staging: codec errors discard staged output; trailing input
// and incomplete final input reject after successful output has been enqueued.
#include "web_platform/compression_stream.h"
#include "web_platform/compression_codec.h"
#include "web_platform/compression_input.h"
#include "js/ArrayBuffer.h"
#include "js/ErrorReport.h"
#include "js/GCVector.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include <cstring>

namespace ruyijs::web_platform {
namespace {
void Finalize(JS::GCContext*,JSObject* object) {
  const auto value=JS::GetReservedSlot(object,0);
  if (!value.isUndefined()) CloseCompressionCodec(static_cast<CompressionCodec*>(value.toPrivate()));
}
const JSClassOps kOperationOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize};
const JSErrorFormatString* ErrorFormat(void* value,unsigned) {return static_cast<JSErrorFormatString*>(value);}
bool CodecError(JSContext* cx,const CompressionStep& step) {
  if (!step.detail) return WebKit::Fail(cx,step.error);
  JSErrorFormatString format={"RUYIJS_COMPRESSION_ERROR",step.error,1,JSEXN_TYPEERR};
  JS_ReportErrorNumberUTF8(cx,ErrorFormat,&format,0,step.detail);return false;
}
bool Run(JSContext* cx,JS::HandleObject operation,JS::HandleObject controller,JS::HandleValue chunk,bool flush) {
  auto* codec=static_cast<CompressionCodec*>(JS::GetReservedSlot(operation,0).toPrivate());
  CompressionBytes input;size_t length=0,offset=0;
  if (!flush && !CompressionInput(cx,chunk,&input,&length)) return false;
  JS::RootedVector<JSObject*> output(cx);bool again;
  do {
    uint8_t buffer[16384];const uint8_t* current=input?input.get()+offset:nullptr;
    const auto step=codec->format==CompressionFormat::Brotli?
        StepBrotli(*codec,current,length-offset,buffer,sizeof(buffer),flush):
        StepZlib(*codec,current,length-offset,buffer,sizeof(buffer),flush);
    if (step.error) return CodecError(cx,step);
    offset+=step.consumed;again=step.again;
    if (step.written) {
      JS::RootedObject view(cx);
      {
        // Firefox's fixed BufferSource callback enters the input realm.
        JS::RootedObject target(cx,flush?controller.get():js::CheckedUnwrapStatic(&chunk.toObject()));
        if (!target) return false;
        JSAutoRealm outputRealm(cx,target);
        view=JS_NewUint8Array(cx,step.written);
        if (!view) return false;
        JS::AutoCheckCannotGC nogc;bool shared=false;
        std::memcpy(JS_GetUint8ArrayData(view,&shared,nogc),buffer,step.written);
      }
      if (!output.append(view)) {JS_ReportOutOfMemory(cx);return false;}
    }
  } while (again);
  for (const auto& view:output) {
    JS::RootedValue value(cx,JS::ObjectValue(*view));
    if (!resource_network::NativeTransformEnqueue(cx,controller,value)) return false;
  }
  if (!codec->decompress) return true;
  if (flush && !codec->ended) return WebKit::Fail(cx,"The input is ended without reaching the stream end");
  if (!flush && codec->ended && offset!=length) return WebKit::Fail(cx,"Unexpected input after the end of stream");
  return true;
}
bool Transform(JSContext* cx,JS::HandleObject operation,JS::HandleObject controller,JS::HandleValue chunk) {
  return Run(cx,operation,controller,chunk,false);
}
bool Flush(JSContext* cx,JS::HandleObject operation,JS::HandleObject controller,JS::HandleValue chunk) {
  return Run(cx,operation,controller,chunk,true);
}
}
const JSClass kCompressionOperationClass={"NativeCompressionOperation",
    JSCLASS_HAS_RESERVED_SLOTS(1)|JSCLASS_FOREGROUND_FINALIZE,&kOperationOps};
const resource_network::NativeTransformSteps kCompressionSteps={Transform,Flush};
}
