#include "web_media/video_frame_copy.h"
#include "web_media/media_runtime.h"
#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/GlobalObject.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/SharedArrayBuffer.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include <cstring>
namespace ruyijs::web_media {
bool FrameCopyBytes(JSContext* cx,JS::HandleObject self,JS::HandleValue destination,const FrameLayout& layout) {
  JS::RootedObject target(cx,destination.isObject()?js::CheckedUnwrapStatic(&destination.toObject()):nullptr);
  if(!target||(!JS_IsArrayBufferViewObject(target)&&!JS::IsArrayBufferObject(target)&&!JS::IsSharedArrayBufferObject(target)))
    return WebKit::Fail(cx,"VideoFrame.copyTo: Destination must be an ArrayBuffer or ArrayBufferView");
  const size_t length=JS_IsArrayBufferViewObject(target)?JS_GetArrayBufferViewByteLength(target):
      JS::IsArrayBufferObject(target)?JS::GetArrayBufferByteLength(target):JS::GetSharedArrayBufferByteLength(target);
  if(length<layout.size)return WebKit::Fail(cx,"VideoFrame.copyTo: Destination buffer is too small");
  std::string sourceFormat;JS::RootedValue format(cx,JS::GetReservedSlot(self,VFFormat));
  if(!ImageString(cx,format,&sourceFormat))return false;
  const bool swap=sourceFormat[0]!=layout.format[0],opaque=layout.format.back()=='X'||sourceFormat.back()=='X';
  JS::RootedObject pixels(cx,ImageObject(self,VFPixels));const uint32_t width=uint32_t(JS::GetReservedSlot(self,VFWidth).toNumber());
  JS::AutoCheckCannotGC nogc;bool shared=false;uint8_t* to=nullptr;
  if(JS_IsArrayBufferViewObject(target))to=static_cast<uint8_t*>(JS_GetArrayBufferViewData(target,&shared,nogc));
  else if(JS::IsSharedArrayBufferObject(target))to=JS::GetSharedArrayBufferData(target,&shared,nogc);
  else to=JS::GetArrayBufferData(target,&shared,nogc);
  const auto* from=JS_GetUint8ArrayData(pixels,&shared,nogc);
  for(uint32_t y=0;y<layout.height;++y)for(uint32_t x=0;x<layout.width;++x) {
    const auto* src=from+(size_t(y+layout.y)*width+x+layout.x)*4;auto* dst=to+layout.offset+size_t(y)*layout.stride+x*4;
    dst[0]=src[swap?2:0];dst[1]=src[1];dst[2]=src[swap?0:2];dst[3]=opaque?255:src[3];
  }
  return true;
}
bool VideoFrameCopy(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);const bool copy=WebKit::Slot(args)==1;
  JS::RootedObject self(cx),caller(cx,JS::CurrentGlobalOrNull(cx));
  if(!ImageBrand(cx,args.thisv(),&kVideoFrameClass,copy?"copyTo":"allocationSize",&self))return copy?RejectMediaException(cx,caller,args.rval()):false;
  JS::RootedObject factories(cx,ImageObject(self,VFFactories));
  if(!ImageObject(self,VFPixels)) {
    ImageException(cx,factories,"InvalidStateError",copy?"VideoFrame.copyTo: No media resource in VideoFrame":"VideoFrame.allocationSize: No media resource in VideoFrame");
    return copy?RejectMediaException(cx,caller,args.rval()):false;
  }
  FrameLayout layout;
  if(!ReadFrameLayout(cx,self,args.get(copy?1:0),copy,&layout))return copy?RejectMediaException(cx,caller,args.rval()):false;
  if(!copy){args.rval().setNumber(layout.size);return true;}
  if(!FrameCopyBytes(cx,self,args.get(0),layout))return RejectMediaException(cx,caller,args.rval());
  JS::RootedObject promise(cx);
  {
  JSAutoRealm realm(cx,self);
  JS::RootedObject plane(cx,JS_NewPlainObject(cx)),list(cx,JS::NewArrayObject(cx,1));promise=JS::NewPromiseObject(cx,nullptr);
  if(!plane||!list||!promise||!JS_DefineProperty(cx,plane,"offset",layout.offset,JSPROP_ENUMERATE)||
      !JS_DefineProperty(cx,plane,"stride",layout.stride,JSPROP_ENUMERATE))return false;
  JS::RootedValue value(cx,JS::ObjectValue(*plane));if(!JS_SetElement(cx,list,0,value))return false;
  value.setObject(*list);if(!JS::ResolvePromise(cx,promise,value))return false;
  }
  if(!JS_WrapObject(cx,&promise))return false;args.rval().setObject(*promise);return true;
}
}
