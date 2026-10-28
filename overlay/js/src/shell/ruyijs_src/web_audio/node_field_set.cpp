#include "web_audio/node_fields.h"
#include <cmath>
#include <cstring>
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/ScalarType.h"
#include "js/String.h"
#include "js/experimental/TypedData.h"

namespace ruyijs::web_audio {
namespace {
bool EnumValid(JSContext* cx,const char* name,JS::HandleString value,bool* valid) {
  const char* types[]={"lowpass","highpass","bandpass","lowshelf","highshelf",
                       "peaking","notch","allpass",nullptr};
  const char* panning[]={"equalpower","HRTF",nullptr};
  const char* distance[]={"linear","inverse","exponential",nullptr};
  const char* oversample[]={"none","2x","4x",nullptr};
  const char** choices=!std::strcmp(name,"type")?types:
    !std::strcmp(name,"panningModel")?panning:
    !std::strcmp(name,"distanceModel")?distance:oversample;
  for(;*choices;++choices) {
    if(!JS_StringEqualsAscii(cx,value,*choices,valid)) return false;
    if(*valid) return true;
  }
  *valid=false;return true;
}
}
bool WriteField(State& s,JS::HandleObject node,int32_t index,JS::HandleValue input) {
  JSContext* cx=s.cx;
  const auto& f=kNodeFields[index];
  JS::RootedObject fields(cx,&JS::GetReservedSlot(node,kNodeFieldsSlot).toObject());
  JS::RootedValue value(cx,input);
  if(f.type==FT::Number) {
    double number=0;
    if(!JS::ToNumber(cx,value,&number)) return false;
    bool valid=std::isfinite(number)&&number>=f.minimum&&number<=f.maximum;
    if(!std::strcmp(f.name,"fftSize"))
      valid=valid && number==std::floor(number) &&
        (uint32_t(number)&(uint32_t(number)-1))==0;
    if(!std::strcmp(f.name,"minDecibels") || !std::strcmp(f.name,"maxDecibels")) {
      const bool minimum=!std::strcmp(f.name,"minDecibels");
      JS::RootedValue other(cx);
      if(!ReadField(cx,node,minimum?"maxDecibels":"minDecibels",&other)) return false;
      valid=valid&&(minimum?number<other.toNumber():number>other.toNumber());
    }
    if(!valid) return ThrowDOMException(s,"IndexSizeError","Audio node value is out of range");
    value.setNumber(number);
  } else if(f.type==FT::Boolean) value.setBoolean(JS::ToBoolean(value));
  else if(f.type==FT::String) {
    JS::RootedString text(cx,JS::ToString(cx,value));
    bool valid=false;
    if(!text || !EnumValid(cx,f.name,text,&valid)) return false;
    if(!valid) return WebKit::Fail(cx,"Invalid audio node enumeration");
    value.setString(text);
  } else if(f.type==FT::Buffer) {
    if(!value.isNull() && (!value.isObject() || JS::GetClass(&value.toObject())!=&kBufferClass))
      return WebKit::Fail(cx,"Audio node buffer must be an AudioBuffer or null");
  } else if(f.type==FT::Curve) {
    if(!value.isNull() && (!value.isObject() ||
       !JS_IsFloat32Array(&value.toObject())))
      return WebKit::Fail(cx,"WaveShaper curve must be a Float32Array or null");
  } else if(f.type==FT::Event) {
    JS::RootedValue old(cx);
    if(!JS_GetElement(cx,fields,uint32_t(index),&old) ||
       !ReplaceHandler(s,node,f.name+2,old,&value)) return false;
  }
  return JS_SetElement(cx,fields,uint32_t(index),value);
}
}
