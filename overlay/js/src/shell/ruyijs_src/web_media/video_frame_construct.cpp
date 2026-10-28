#include "web_media/image_decoder.h"
#include "runtime/native_constructor_trace.h"
#include "js/Conversions.h"
#include "js/GlobalObject.h"
#include "js/ForOfIterator.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include <cmath>
#include <cstring>
namespace ruyijs::web_media {
namespace {
bool Member(JSContext* cx,JS::HandleObject dictionary,const char* name,bool required,JS::MutableHandleValue value) {
  value.setUndefined();if(dictionary&&!JS_GetProperty(cx,dictionary,name,value))return false;
  if(required&&value.isUndefined())return WebKit::Fail(cx,(std::string("VideoFrame constructor: Missing required '")+name+"' member of VideoFrameBufferInit.").c_str());
  return true;
}
bool Display(JSContext* cx,JS::HandleObject init,uint32_t* width,uint32_t* height,bool* hasWidth,bool* hasHeight) {
  JS::RootedValue value(cx);
  if(!Member(cx,init,"displayHeight",false,&value))return false;
  *hasHeight=!value.isUndefined();if(*hasHeight&&!ImageUnsigned(cx,value,"displayHeight",height))return false;
  if(!Member(cx,init,"displayWidth",false,&value))return false;
  *hasWidth=!value.isUndefined();return !*hasWidth||ImageUnsigned(cx,value,"displayWidth",width);
}
}
bool VideoFrameConstructor(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
  if(!args.isConstructing())return WebKit::Fail(cx,"VideoFrame constructor: 'new' is required");
  JS::RootedObject prototype(cx),factories(cx),global(cx,JS::CurrentGlobalOrNull(cx));
  if(!ImageDesiredPrototype(cx,args,IFFrame,&prototype)||!ImageFactories(cx,global,&factories))return false;
  JS::RootedObject source(cx,args.get(0).isObject()?js::CheckedUnwrapStatic(&args[0].toObject()):nullptr);
  const bool frame=source&&JS::GetClass(source)==&kVideoFrameClass;
  if(!args.get(1).isNullOrUndefined()&&!args.get(1).isObject())return WebKit::Fail(cx,"VideoFrame constructor: Argument 2 can't be converted to a dictionary.");
  JS::RootedObject init(cx,args.get(1).isObject()?&args[1].toObject():nullptr),object(cx),colorPrototype(cx),color(cx);
  JS::RootedValue value(cx),colorInit(cx),rectInit(cx);double timestamp=0,duration=0;bool hasDuration=false;
  uint32_t displayWidth=0,displayHeight=0;bool hasDisplayWidth=false,hasDisplayHeight=false;
  if(frame) {
    if(!Member(cx,init,"alpha",false,&value))return false;
    std::string alpha="keep";if(!value.isUndefined()&&!ImageString(cx,value,&alpha))return false;
    if(alpha!="keep"&&alpha!="discard")return WebKit::Fail(cx,("VideoFrame constructor: '"+alpha+"' (value of 'alpha' member of VideoFrameInit) is not a valid value for enumeration AlphaOption.").c_str());
    if(!Display(cx,init,&displayWidth,&displayHeight,&hasDisplayWidth,&hasDisplayHeight))return false;
    if(!ImageObject(source,VFPixels))return ImageException(cx,factories,"InvalidStateError","VideoFrame constructor: No media resource in the VideoFrame now");
    object=JS_NewObjectWithGivenProto(cx,&kVideoFrameClass,prototype);if(!object)return false;
    for(unsigned slot=0;slot<VFSlots;++slot){value.set(JS::GetReservedSlot(source,slot));if(!JS_WrapValue(cx,&value))return false;JS::SetReservedSlot(object,slot,value);}
    JS::SetReservedSlot(object,VFFactories,JS::ObjectValue(*factories));
    if(alpha=="discard") {
      std::string format;value.set(JS::GetReservedSlot(object,VFFormat));if(!ImageString(cx,value,&format))return false;
      format.back()='X';JS::RootedString text(cx,JS_NewStringCopyZ(cx,format.c_str()));if(!text)return false;
      JS::SetReservedSlot(object,VFFormat,JS::StringValue(text));
    }
    if(!Member(cx,init,"duration",false,&value))return false;if(!value.isUndefined()){if(!JS::ToNumber(cx,value,&duration))return false;JS::SetReservedSlot(object,VFDuration,JS::NumberValue(std::trunc(duration)));}
    if(!Member(cx,init,"timestamp",false,&value))return false;if(!value.isUndefined()){if(!JS::ToNumber(cx,value,&timestamp))return false;JS::SetReservedSlot(object,VFTimestamp,JS::NumberValue(std::trunc(timestamp)));}
    if(!Member(cx,init,"visibleRect",false,&rectInit))return false;
  } else {
    std::vector<uint8_t> bytes;if(!ImageBuffer(cx,args.get(0),&bytes))return false;
    uint32_t width=0,height=0;
    if(!Member(cx,init,"codedHeight",true,&value)||!ImageUnsigned(cx,value,"codedHeight",&height)||
        !Member(cx,init,"codedWidth",true,&value)||!ImageUnsigned(cx,value,"codedWidth",&width))return false;
    if(!Member(cx,init,"colorSpace",false,&colorInit))return false;
    if(!ImageFactoryPrototype(cx,factories,IFColor,&colorPrototype)||!NewVideoColorSpaceFromInit(cx,colorPrototype,colorInit,true,&color))return false;
    if(!Display(cx,init,&displayWidth,&displayHeight,&hasDisplayWidth,&hasDisplayHeight))return false;
    if(!Member(cx,init,"duration",false,&value))return false;
    hasDuration=!value.isUndefined();if(hasDuration&&!JS::ToNumber(cx,value,&duration))return false;
    std::string format;if(!Member(cx,init,"format",true,&value)||!ImageString(cx,value,&format))return false;
    if(format!="RGBA"&&format!="RGBX"&&format!="BGRA"&&format!="BGRX")return ImageException(cx,factories,"NotSupportedError","VideoFrame constructor: Unsupported pixel format");
    if(!Member(cx,init,"layout",false,&value))return false;
    if(!value.isUndefined()) {
      JS::ForOfIterator iterator(cx);if(!iterator.init(value))return false;bool done=false;
      while(true){if(!iterator.next(&value,&done))return false;if(done)break;
        if(!value.isObject())return WebKit::Fail(cx,"VideoFrame constructor: PlaneLayout must be a dictionary");
        JS::RootedObject plane(cx,&value.toObject());uint32_t converted;
        if(!Member(cx,plane,"offset",true,&value)||!ImageUnsigned(cx,value,"offset",&converted)||
            !Member(cx,plane,"stride",true,&value)||!ImageUnsigned(cx,value,"stride",&converted))return false;
      }
    }
    if(!Member(cx,init,"timestamp",true,&value)||!JS::ToNumber(cx,value,&timestamp)||!Member(cx,init,"visibleRect",false,&rectInit))return false;
    if(!std::isfinite(timestamp)||timestamp<-9223372036854775808.0||timestamp>=9223372036854775808.0)
      return WebKit::Fail(cx,"VideoFrame constructor: timestamp is outside the long long range");
    if(hasDuration&&(!std::isfinite(duration)||duration<0||duration>=18446744073709551616.0))return WebKit::Fail(cx,"VideoFrame constructor: duration is outside the unsigned long long range");
    if(!width||!height||uint64_t(width)*height>domain::web_media::kImagePixelLimit||uint64_t(width)*height*4>bytes.size())
      return WebKit::Fail(cx,"VideoFrame constructor: Buffer size is too small for the coded dimensions");
    object=JS_NewObjectWithGivenProto(cx,&kVideoFrameClass,prototype);
    JS::RootedObject pixels(cx,JS_NewUint8Array(cx,size_t(width)*height*4));JS::RootedString formatString(cx,JS_NewStringCopyZ(cx,format.c_str()));
    if(!object||!pixels||!formatString)return false;
    {JS::AutoCheckCannotGC nogc;bool shared=false;std::memcpy(JS_GetUint8ArrayData(pixels,&shared,nogc),bytes.data(),size_t(width)*height*4);}
    JS::SetReservedSlot(object,VFPixels,JS::ObjectValue(*pixels));JS::SetReservedSlot(object,VFFormat,JS::StringValue(formatString));
    JS::SetReservedSlot(object,VFWidth,JS::NumberValue(width));JS::SetReservedSlot(object,VFHeight,JS::NumberValue(height));
    JS::SetReservedSlot(object,VFDisplayWidth,JS::NumberValue(width));JS::SetReservedSlot(object,VFDisplayHeight,JS::NumberValue(height));
    JS::SetReservedSlot(object,VFTimestamp,JS::NumberValue(std::trunc(timestamp)));JS::SetReservedSlot(object,VFDuration,hasDuration?JS::NumberValue(std::trunc(duration)):JS::NullValue());
    JS::SetReservedSlot(object,VFFactories,JS::ObjectValue(*factories));JS::SetReservedSlot(object,VFColor,JS::ObjectValue(*color));
    JS::SetReservedSlot(object,VFX,JS::Int32Value(0));JS::SetReservedSlot(object,VFY,JS::Int32Value(0));
    JS::SetReservedSlot(object,VFW,JS::NumberValue(width));JS::SetReservedSlot(object,VFH,JS::NumberValue(height));
  }
  if(!rectInit.isNullOrUndefined()) {
    if(!rectInit.isObject())return WebKit::Fail(cx,"VideoFrame constructor: visibleRect must be a dictionary");
    JS::RootedObject rect(cx,&rectInit.toObject());uint32_t fields[4]={0,0,0,0};
    const char* names[]={"x","y","width","height"};
    for(unsigned i:{3u,2u,0u,1u}) {
      if(!JS_GetProperty(cx,rect,names[i],&value))return false;
      if(!value.isUndefined()&&!ImageUnsigned(cx,value,names[i],&fields[i]))return false;
    }
    if(!fields[2]||!fields[3]||uint64_t(fields[0])+fields[2]>JS::GetReservedSlot(object,VFWidth).toNumber()||
        uint64_t(fields[1])+fields[3]>JS::GetReservedSlot(object,VFHeight).toNumber())return WebKit::Fail(cx,"VideoFrame constructor: Invalid visibleRect");
    for(unsigned i=0;i<4;++i)JS::SetReservedSlot(object,VFX+i,JS::NumberValue(fields[i]));
    JS::SetReservedSlot(object,VFDisplayWidth,JS::NumberValue(fields[2]));JS::SetReservedSlot(object,VFDisplayHeight,JS::NumberValue(fields[3]));
  }
  if((hasDisplayWidth&&!displayWidth)||(hasDisplayHeight&&!displayHeight))return WebKit::Fail(cx,"VideoFrame display dimensions must be positive");
  if(hasDisplayWidth)JS::SetReservedSlot(object,VFDisplayWidth,JS::NumberValue(displayWidth));
  if(hasDisplayHeight)JS::SetReservedSlot(object,VFDisplayHeight,JS::NumberValue(displayHeight));
  args.rval().setObject(*object);return trace.Finish(true);
}
}
