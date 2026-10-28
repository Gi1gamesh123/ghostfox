#include "web_media/image_decoder.h"
#include "web_geometry/internal.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include <cstring>
namespace ruyijs::web_media {
const JSClass kVideoFrameClass={"VideoFrame",JSCLASS_HAS_RESERVED_SLOTS(VFSlots)};
bool NewVideoFrame(JSContext* cx,JS::HandleObject factories,
    const domain::web_media::ImageFrame& image,JS::MutableHandleObject result) {
  JS::RootedObject prototype(cx),pixels(cx,JS_NewUint8Array(cx,image.pixels.size())),colorPrototype(cx),color(cx);
  if(!pixels||!ImageFactoryPrototype(cx,factories,IFFrame,&prototype)||
      !ImageFactoryPrototype(cx,factories,IFColor,&colorPrototype)||!NewVideoColorSpace(cx,colorPrototype,false,&color))return false;
  result.set(JS_NewObjectWithGivenProto(cx,&kVideoFrameClass,prototype));if(!result)return false;
  {JS::AutoCheckCannotGC nogc;bool shared=false;auto* data=JS_GetUint8ArrayData(pixels,&shared,nogc);
    std::memcpy(data,image.pixels.data(),image.pixels.size());}
  JS::RootedString format(cx,JS_NewStringCopyZ(cx,image.opaque?"BGRX":"BGRA"));if(!format)return false;
  JS::SetReservedSlot(result,VFPixels,JS::ObjectValue(*pixels));JS::SetReservedSlot(result,VFFormat,JS::StringValue(format));
  JS::SetReservedSlot(result,VFWidth,JS::NumberValue(image.width));JS::SetReservedSlot(result,VFHeight,JS::NumberValue(image.height));
  JS::SetReservedSlot(result,VFDisplayWidth,JS::NumberValue(image.width));JS::SetReservedSlot(result,VFDisplayHeight,JS::NumberValue(image.height));
  JS::SetReservedSlot(result,VFTimestamp,JS::NumberValue(double(image.timestamp)));
  JS::SetReservedSlot(result,VFDuration,image.animated?JS::NumberValue(double(image.duration)):JS::NullValue());
  JS::SetReservedSlot(result,VFFactories,JS::ObjectValue(*factories));JS::SetReservedSlot(result,VFColor,JS::ObjectValue(*color));
  JS::SetReservedSlot(result,VFX,JS::Int32Value(0));JS::SetReservedSlot(result,VFY,JS::Int32Value(0));
  JS::SetReservedSlot(result,VFW,JS::NumberValue(image.width));JS::SetReservedSlot(result,VFH,JS::NumberValue(image.height));return true;
}
bool VideoFrameGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);const unsigned field=unsigned(WebKit::Slot(args));
  static const char* names[]={"get format","get codedWidth","get codedHeight","get codedRect","get visibleRect", "get displayWidth","get displayHeight","get duration","get timestamp","get colorSpace"};
  static const unsigned slots[]={VFFormat,VFWidth,VFHeight,VFX,VFX,VFDisplayWidth,VFDisplayHeight,VFDuration,VFTimestamp,VFColor};
  JS::RootedObject self(cx);if(!ImageBrand(cx,args.thisv(),&kVideoFrameClass,names[field],&self))return false;
  const bool open=ImageObject(self,VFPixels);
  if(field==3||field==4) {
    if(!open){args.rval().setNull();return true;}
    {JSAutoRealm realm(cx,self);
      JS::RootedObject factories(cx,ImageObject(self,VFFactories)),prototype(cx);
      if(!ImageFactoryPrototype(cx,factories,IFRect,&prototype))return false;
      double rect[4]={0,0,JS::GetReservedSlot(self,VFWidth).toNumber(),JS::GetReservedSlot(self,VFHeight).toNumber()};
      if(field==4)for(unsigned i=0;i<4;++i)rect[i]=JS::GetReservedSlot(self,VFX+i).toNumber();
      if(!web_geometry::NewValueObject(cx,&web_geometry::kRectReadOnlyClass,prototype,rect,4,args.rval()))return false;}
    return JS_WrapValue(cx,args.rval());
  }
  args.rval().set(JS::GetReservedSlot(self,slots[field]));return JS_WrapValue(cx,args.rval());
}
bool VideoFrameMethod(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);const bool close=WebKit::Slot(args)==3;
  JS::RootedObject self(cx);if(!ImageBrand(cx,args.thisv(),&kVideoFrameClass,close?"close":"clone",&self))return false;
  JS::RootedObject factories(cx,ImageObject(self,VFFactories));
  if(close) {
    JSAutoRealm realm(cx,self);
    JS::RootedObject colorPrototype(cx),color(cx);
    if(!ImageFactoryPrototype(cx,factories,IFColor,&colorPrototype)||!NewVideoColorSpace(cx,colorPrototype,false,&color))return false;
    JS::SetReservedSlot(self,VFPixels,JS::NullValue());JS::SetReservedSlot(self,VFFormat,JS::NullValue());
    for(unsigned slot:{VFWidth,VFHeight,VFDisplayWidth,VFDisplayHeight,VFX,VFY,VFW,VFH})JS::SetReservedSlot(self,slot,JS::Int32Value(0));
    JS::SetReservedSlot(self,VFColor,JS::ObjectValue(*color));args.rval().setUndefined();return true;
  }
  if(!ImageObject(self,VFPixels))return ImageException(cx,factories,"InvalidStateError","VideoFrame.clone: No media resource in the VideoFrame now");
  JS::RootedObject prototype(cx),result(cx);
  {JSAutoRealm realm(cx,self);
    if(!ImageFactoryPrototype(cx,factories,IFFrame,&prototype))return false;
    result=JS_NewObjectWithGivenProto(cx,&kVideoFrameClass,prototype);if(!result)return false;
    for(unsigned slot=0;slot<VFSlots;++slot)JS::SetReservedSlot(result,slot,JS::GetReservedSlot(self,slot));}
  if(!JS_WrapObject(cx,&result))return false;args.rval().setObject(*result);return true;
}
}
