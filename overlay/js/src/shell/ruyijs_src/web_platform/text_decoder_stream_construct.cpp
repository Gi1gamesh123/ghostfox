// Original encoding_rs label resolution and intrinsic Transform construction.
#include "web_platform/text_decoder_stream.h"
#include "web_platform/text_codec.h"
#include "web_platform/text_decoder_input.h"
#include "runtime/native_constructor_trace.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs::web_platform {
namespace {
bool Label(JSContext* cx,JS::HandleValue value,std::string* result) {
  JS::RootedString text(cx,value.isUndefined()?JS_NewStringCopyZ(cx,"utf-8"):JS::ToString(cx,value));
  if (!text) return false;
  JSLinearString* linear=JS_EnsureLinearString(cx,text);
  if (!linear) return false;
  const size_t length=JS::GetDeflatedUTF8StringLength(linear);
  JS::UniqueChars bytes(JS_EncodeStringToUTF8(cx,text));
  if (!bytes) return false;
  const auto status=CopyDecoderBytes(bytes.get(),length,result);
  if (status==DecodeStatus::OutOfMemory) {JS_ReportOutOfMemory(cx);return false;}
  return status==DecodeStatus::Ok || WebKit::FailKind(cx,"TextDecoderStream label exceeds the bounded text limit",JSEXN_RANGEERR);
}
const JSErrorFormatString* LabelError(void* data,unsigned) {return static_cast<const JSErrorFormatString*>(data);}
bool Unsupported(JSContext* cx,const std::string& label) {
  size_t begin=label.find_first_not_of(" \t\n\f\r"),end=label.find_last_not_of(" \t\n\f\r");
  if (begin==std::string::npos) {begin=0;end=0;} else ++end;
  std::string trimmed;
  const auto status=CopyDecoderBytes(label.data()+begin,end-begin,&trimmed);
  if (status!=DecodeStatus::Ok) {JS_ReportOutOfMemory(cx);return false;}
  JSErrorFormatString format={"RUYIJS_DECODER_STREAM_LABEL","TextDecoderStream constructor: The given encoding '{0}' is not supported.",1,JSEXN_RANGEERR};
  JS_ReportErrorNumberUTF8(cx,LabelError,&format,0,trimmed.c_str());return false;
}
}
bool TextDecoderStreamConstructor(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
  if (!args.isConstructing()) return WebKit::Fail(cx,"TextDecoderStream constructor: 'new' is required");
  State& state=*static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject prototype(cx),factory(cx),object(cx),transform(cx),readable(cx),writable(cx);
  if (!DecoderStreamPrototype(cx,args,&prototype)) return false;
  std::string label,encoding;
  if (!Label(cx,args.get(0),&label)) return false;
  JS::RootedObject options(cx);
  if (!DecoderOptions(cx,args.get(1),&options,true)) return false;
  bool fatal=false,ignore=false;
  if (options) {
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx,options,"fatal",&value)) return false;fatal=JS::ToBoolean(value);
    if (!JS_GetProperty(cx,options,"ignoreBOM",&value)) return false;ignore=JS::ToBoolean(value);
  }
  const auto status=DecoderEncodingFromLabel(label,&encoding);
  if (status==DecodeStatus::OutOfMemory) {JS_ReportOutOfMemory(cx);return false;}
  if (status!=DecodeStatus::Ok) return Unsupported(cx,label);
  if (!resource_network::NativeTransformFactory(cx,state.kit.page,&factory)) return false;
  object=JS_NewObjectWithGivenProto(cx,&kTextDecoderStreamClass,prototype);
  JS::RootedString name(cx,JS_NewStringCopyN(cx,encoding.data(),encoding.size()));
  if (!object || !name) return false;
  void* decoder=OpenTextDecoder(encoding,ignore);
  if (!decoder) {JS_ReportOutOfMemory(cx);return false;}
  JS::SetReservedSlot(object,DSHandle,JS::PrivateValue(decoder));
  JS::SetReservedSlot(object,DSEncoding,JS::StringValue(name));
  JS::SetReservedSlot(object,DSFatal,JS::BooleanValue(fatal));
  JS::SetReservedSlot(object,DSIgnoreBom,JS::BooleanValue(ignore));
  if (!resource_network::NewNativeTransform(cx,factory,object,kDecoderStreamSteps,&transform,&readable,&writable)) return false;
  JS::SetReservedSlot(object,DSTransform,JS::ObjectValue(*transform));
  JS::SetReservedSlot(object,DSReadable,JS::ObjectValue(*readable));
  JS::SetReservedSlot(object,DSWritable,JS::ObjectValue(*writable));
  args.rval().setObject(*object);return trace.Finish(true);
}
}
