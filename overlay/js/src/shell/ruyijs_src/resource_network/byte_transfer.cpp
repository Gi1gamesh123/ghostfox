// Only intrinsic buffers/views are used; transfer owns memory across failures.
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/Realm.h"
#include "js/experimental/TypedData.h"
#include "mozilla/UniquePtr.h"
#include <cmath>

namespace ruyijs::resource_network {
namespace {
const JSErrorFormatString* ByteErrorFormat(void* data,unsigned) {
  return static_cast<const JSErrorFormatString*>(data);
}
bool ConversionError(JSContext* cx,const char* prefix,const char* suffix) {
  JSErrorFormatString format={"RUYIJS_BYTE_CONVERSION","{0}{1}",2,JSEXN_TYPEERR};
  JS_ReportErrorNumberASCII(cx,ByteErrorFormat,&format,0,prefix,suffix);return false;
}
}
bool EnforceU64(JSContext* cx,JS::HandleValue value,uint64_t* out,const char* message) {
  double number;
  if (!JS::ToNumber(cx,value,&number)) return false;
  if (!std::isfinite(number)) return ConversionError(cx,message," is not a finite value, so is out of range for unsigned long long.");
  number=std::trunc(number);
  if (number<0 || number>=18446744073709551616.0)
    return ConversionError(cx,message," is out of range for unsigned long long.");
  *out=static_cast<uint64_t>(number);return true;
}
bool ConvertByteView(JSContext* cx,JS::HandleValue value,ByteView* info,
                      JS::MutableHandleObject buffer,const char* message) {
  JS::RootedObject view(cx,value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
  if (!view || !JS_IsArrayBufferViewObject(view)) return ConversionError(cx,message,": Argument 1 does not implement interface ArrayBufferView.");
  bool shared=false;
  {
    JSAutoRealm realm(cx,view);
    buffer.set(JS_GetArrayBufferViewBuffer(cx,view,&shared));
    if (!buffer) return false;
  }
  if (shared) return ConversionError(cx,message,": Argument 1 can't be a SharedArrayBuffer or an ArrayBufferView backed by a SharedArrayBuffer");
  if (JS::IsResizableArrayBufferView(view)) return ConversionError(cx,message,": Argument 1 can't be a resizable ArrayBuffer or ArrayBufferView");
  info->bufferLength=JS::GetArrayBufferByteLength(buffer);
  info->offset=JS_GetArrayBufferViewByteOffset(view);
  info->length=JS_GetArrayBufferViewByteLength(view);
  info->type=JS_GetArrayBufferViewType(view);
  info->element=info->type==JS::Scalar::MaxTypedArrayViewType?1:JS::Scalar::byteSize(info->type);
  return JS_WrapObject(cx,buffer);
}
bool TransferByteBuffer(JSContext* cx,JS::HandleObject input,JS::MutableHandleObject result) {
  JS::RootedObject buffer(cx,JS::UnwrapArrayBuffer(input));
  if (!buffer) return Error(cx,"Byte source requires an ArrayBuffer");
  size_t length=0;
  mozilla::UniquePtr<void,JS::FreePolicy> data;
  {
    JSAutoRealm realm(cx,buffer);
    if (JS::IsDetachedArrayBufferObject(buffer)) return Error(cx,"Detached ArrayBuffer");
    length=JS::GetArrayBufferByteLength(buffer);
    data.reset(JS::StealArrayBufferContents(cx,buffer));
    if (JS_IsExceptionPending(cx) || !JS::DetachArrayBuffer(cx,buffer)) return false;
  }
  result.set(JS::NewArrayBufferWithContents(cx,length,std::move(data)));
  return result.get()!=nullptr;
}
bool NewByteView(JSContext* cx,JS::Scalar::Type type,JS::HandleObject input,
                 uint64_t offset,uint64_t length,JS::MutableHandleObject result) {
  JS::RootedObject buffer(cx,input);
  if (!JS_WrapObject(cx,&buffer)) return false;
  switch (type) {
    case JS::Scalar::MaxTypedArrayViewType:
      result.set(JS_NewDataView(cx,buffer,size_t(offset),size_t(length)));break;
#define MAKE_VIEW(ExternalT,NativeT,Name) \
    case JS::Scalar::Name: result.set(JS_New##Name##ArrayWithBuffer(cx,buffer,size_t(offset),int64_t(length)));break;
    JS_FOR_EACH_TYPED_ARRAY(MAKE_VIEW)
#undef MAKE_VIEW
    default: return Error(cx,"Unknown byte stream view type");
  }
  return result.get()!=nullptr;
}
bool StoreByteBuffer(JSContext* cx,JS::HandleObject owner,unsigned slot,JS::HandleObject buffer) {
  JSAutoRealm realm(cx,owner);
  JS::RootedObject wrapped(cx,buffer);
  if (!JS_WrapObject(cx,&wrapped)) return false;
  JS::SetReservedSlot(owner,slot,JS::ObjectValue(*wrapped));return true;
}
bool CopyByteRegion(JSContext* cx,JS::HandleObject destination,uint64_t at,
                    JS::HandleObject source,uint64_t offset,uint64_t length) {
  JS::RootedObject to(cx,JS::UnwrapArrayBuffer(destination)),from(cx,JS::UnwrapArrayBuffer(source));
  if (!to || !from || JS::IsDetachedArrayBufferObject(to) || JS::IsDetachedArrayBufferObject(from))
    return Error(cx,"Detached byte queue buffer");
  if (at>JS::GetArrayBufferByteLength(to) || length>JS::GetArrayBufferByteLength(to)-at ||
      offset>JS::GetArrayBufferByteLength(from) || length>JS::GetArrayBufferByteLength(from)-offset)
    return Error(cx,"Byte queue buffer bounds",true);
  return JS::ArrayBufferCopyData(cx,to,size_t(at),from,size_t(offset),size_t(length));
}
}
