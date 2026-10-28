#include "web_platform/compression_stream.h"
#include "web_platform/compression_codec.h"
#include "runtime/native_constructor_trace.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include <cstring>

namespace ruyijs::web_platform {
namespace {
const JSErrorFormatString* FormatError(void* value,unsigned) {return static_cast<JSErrorFormatString*>(value);}
bool Format(JSContext* cx,JS::HandleValue value,bool decompress,CompressionFormat* result) {
  JS::RootedString text(cx,JS::ToString(cx,value));if (!text) return false;
  for (const auto& entry: {std::pair{"deflate",CompressionFormat::Deflate},
      std::pair{"deflate-raw",CompressionFormat::Raw},std::pair{"gzip",CompressionFormat::Gzip},
      std::pair{"brotli",CompressionFormat::Brotli}}) {
    bool same=false;if (!JS_StringEqualsAscii(cx,text,entry.first,&same)) return false;
    if (same) {*result=entry.second;return true;}
  }
  JS::UniqueChars bytes(JS_EncodeStringToUTF8(cx,text));if (!bytes) return false;
  JSErrorFormatString error={"RUYIJS_COMPRESSION_FORMAT",
      decompress?"DecompressionStream constructor: '{0}' (value of argument 1) is not a valid value for enumeration CompressionFormat.":
      "CompressionStream constructor: '{0}' (value of argument 1) is not a valid value for enumeration CompressionFormat.",1,JSEXN_TYPEERR};
  JS_ReportErrorNumberUTF8(cx,FormatError,&error,0,bytes.get());return false;
}
bool Construct(JSContext* cx,unsigned argc,JS::Value* vp,bool decompress) {
  auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
  if (!args.isConstructing()) return WebKit::Fail(cx,decompress?
      "DecompressionStream constructor: 'new' is required":"CompressionStream constructor: 'new' is required");
  State& state=*static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject prototype(cx),factory(cx),operation(cx),object(cx),transform(cx),readable(cx),writable(cx);
  if (!CompressionPrototype(cx,args,decompress,&prototype)) return false;
  if (argc<1) return WebKit::Fail(cx,decompress?
      "DecompressionStream constructor: At least 1 argument required, but only 0 passed":
      "CompressionStream constructor: At least 1 argument required, but only 0 passed");
  CompressionFormat format;if (!Format(cx,args[0],decompress,&format)) return false;
  if (!resource_network::NativeTransformFactory(cx,state.kit.page,&factory)) return false;
  operation=JS_NewObjectWithGivenProto(cx,&kCompressionOperationClass,nullptr);
  if (!operation) return false;
  auto* codec=OpenCompressionCodec(format,decompress);
  if (!codec) {JS_ReportOutOfMemory(cx);return false;}
  JS::SetReservedSlot(operation,0,JS::PrivateValue(codec));
  if (!resource_network::NewNativeTransform(cx,factory,operation,kCompressionSteps,&transform,&readable,&writable)) return false;
  object=JS_NewObjectWithGivenProto(cx,decompress?&kDecompressionClass:&kCompressionClass,prototype);
  if (!object) return false;
  JS::SetReservedSlot(object,CSTransform,JS::ObjectValue(*transform));
  JS::SetReservedSlot(object,CSReadable,JS::ObjectValue(*readable));
  JS::SetReservedSlot(object,CSWritable,JS::ObjectValue(*writable));
  args.rval().setObject(*object);return trace.Finish(true);
}
}
bool CompressionConstructor(JSContext* cx,unsigned argc,JS::Value* vp) {return Construct(cx,argc,vp,false);}
bool DecompressionConstructor(JSContext* cx,unsigned argc,JS::Value* vp) {return Construct(cx,argc,vp,true);}
}
