#include "web_media/video_frame_copy.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/PropertyAndElement.h"
#include <cmath>
namespace ruyijs::web_media {
bool ReadFrameLayout(JSContext* cx,JS::HandleObject frame,JS::HandleValue input,bool copy,FrameLayout* layout) {
  const std::string prefix=copy?"VideoFrame.copyTo: ":"VideoFrame.allocationSize: ";
  layout->x=uint32_t(JS::GetReservedSlot(frame,VFX).toNumber());layout->y=uint32_t(JS::GetReservedSlot(frame,VFY).toNumber());
  layout->width=uint32_t(JS::GetReservedSlot(frame,VFW).toNumber());layout->height=uint32_t(JS::GetReservedSlot(frame,VFH).toNumber());
  JS::RootedValue value(cx,JS::GetReservedSlot(frame,VFFormat));if(!ImageString(cx,value,&layout->format))return false;
  if(!input.isObject()&&!input.isNullOrUndefined())return WebKit::Fail(cx,"VideoFrame options must be a dictionary");
  JS::RootedObject object(cx,input.isObject()?&input.toObject():nullptr);JS::RootedValue planes(cx);
  if(object) {
    if(!JS_GetProperty(cx,object,"colorSpace",&value))return false;
    if(!value.isUndefined()) {std::string color;if(!ImageString(cx,value,&color))return false;
      if(color!="srgb"&&color!="display-p3")return WebKit::Fail(cx,(prefix+"'"+color+"' (value of 'colorSpace' member of VideoFrameCopyToOptions) is not a valid value for enumeration PredefinedColorSpace.").c_str());
      }
    if(!JS_GetProperty(cx,object,"format",&value))return false;
    if(!value.isUndefined()) {
      if(!ImageString(cx,value,&layout->format))return false;
      bool valid=false;for(const char* format:{"I420","I420A","I422","I422A","I444","I444A","NV12","RGBA","RGBX","BGRA","BGRX","I420P10","I420P12","I420AP10","I420AP12","I422P10","I422P12","I422AP10","I422AP12","I444P10","I444P12","I444AP10","I444AP12"})valid|=layout->format==format;
      if(!valid)return WebKit::Fail(cx,(prefix+"'"+layout->format+"' (value of 'format' member of VideoFrameCopyToOptions) is not a valid value for enumeration VideoPixelFormat.").c_str());
    }
    if(!JS_GetProperty(cx,object,"layout",&planes)||!JS_GetProperty(cx,object,"rect",&value))return false;
    if(!value.isNullOrUndefined()) {
      if(!value.isObject())return WebKit::Fail(cx,"VideoFrame rect must be a dictionary");
      JS::RootedObject rect(cx,&value.toObject());uint32_t* fields[]={&layout->height,&layout->width,&layout->x,&layout->y};
      const char* names[]={"height","width","x","y"};
      for(unsigned i=0;i<4;++i){if(!JS_GetProperty(cx,rect,names[i],&value))return false;
        if(value.isUndefined())*fields[i]=0;
        else {double number;if(!JS::ToNumber(cx,value,&number))return false;
          if(number<0)return WebKit::Fail(cx,((copy?"":prefix)+"rect's "+names[i]+" must be non-negative").c_str());
          if(!std::isfinite(number)||number>UINT32_MAX)return WebKit::Fail(cx,"VideoFrame copy rectangle is outside the coded frame");
          *fields[i]=uint32_t(number);}}
    }
  }
  if(layout->format!="RGBA"&&layout->format!="RGBX"&&layout->format!="BGRA"&&layout->format!="BGRX") {
    JS::RootedObject factories(cx,ImageObject(frame,VFFactories));
    return ImageException(cx,factories,"NotSupportedError",((copy?"":prefix)+layout->format+" is unsupported in ParseVideoFrameCopyToOptions").c_str());
  }
  const uint32_t width=uint32_t(JS::GetReservedSlot(frame,VFWidth).toNumber()),height=uint32_t(JS::GetReservedSlot(frame,VFHeight).toNumber());
  if(!layout->width||!layout->height||uint64_t(layout->x)+layout->width>width||uint64_t(layout->y)+layout->height>height)
    return WebKit::Fail(cx,"VideoFrame copy rectangle is outside the coded frame");
  layout->stride=layout->width*4;
  if(!planes.isUndefined()) {
    JS::ForOfIterator iterator(cx);if(!iterator.init(planes))return false;
    bool done=false;if(!iterator.next(&value,&done))return false;
    if(done||!value.isObject())return WebKit::Fail(cx,"VideoFrame copy requires one plane layout");
    JS::RootedObject plane(cx,&value.toObject());
    if(!JS_GetProperty(cx,plane,"offset",&value)||!ImageUnsigned(cx,value,"offset",&layout->offset)||
        !JS_GetProperty(cx,plane,"stride",&value)||!ImageUnsigned(cx,value,"stride",&layout->stride)||!iterator.next(&value,&done))return false;
    if(!done||layout->stride<layout->width*4)return WebKit::Fail(cx,"Invalid VideoFrame plane layout");
  }
  const uint64_t size=uint64_t(layout->offset)+uint64_t(layout->height)*layout->stride;
  if(size>UINT32_MAX)return WebKit::Fail(cx,"VideoFrame allocation size exceeds unsigned long range");
  layout->size=uint32_t(size);return true;
}
}
