// Copy once while pinned; JS allocations never retain a raw backing pointer.
#include "web_platform/compression_input.h"
#include "web_platform/buffer_source.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/ArrayBuffer.h"
#include "js/ErrorReport.h"
#include "js/experimental/TypedData.h"
#include <cstring>

namespace ruyijs::web_platform {
bool CompressionInput(JSContext* cx,JS::HandleValue chunk,CompressionBytes* bytes,size_t* length) {
  JS::RootedObject input(cx),backing(cx);bool shared=false;
  if (!PlatformBufferSource(cx,chunk,&input,true)) return false;
  const bool buffer=JS::IsArrayBufferObject(input);
  backing=buffer?input.get():JS_GetArrayBufferViewBuffer(cx,input,&shared);
  if (!backing) return false;
  *length=JS::IsDetachedArrayBufferObject(backing)?0:
      buffer?JS::GetArrayBufferByteLength(input):JS_GetArrayBufferViewByteLength(input);
  if (!*length) return true;
  // The original zlib C ABI consumes a uint32 length for one transform.
  if (*length>UINT32_MAX) return WebKit::FailKind(cx,"Compression input exceeds the codec chunk limit",JSEXN_RANGEERR);
  bytes->reset(static_cast<uint8_t*>(std::malloc(*length)));
  if (!*bytes) {JS_ReportOutOfMemory(cx);return false;}
  {
    JS::AutoCheckCannotGC nogc;
    const void* source=buffer?JS::GetArrayBufferData(input,&shared,nogc):JS_GetArrayBufferViewData(input,&shared,nogc);
    if (source) std::memcpy(bytes->get(),source,*length);
    else *length=0;
  }
  return true;
}
}
