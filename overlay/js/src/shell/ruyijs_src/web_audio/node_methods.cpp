#include "web_audio/node_fields.h"
#include <algorithm>
#include <cmath>
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/experimental/TypedData.h"

namespace ruyijs::web_audio {
// Slots 0..3 are the public ruyijs analyser's silence output, 4/5 filter no-ops,
// 6/7 panner coordinates and 8 oscillator periodic-wave entry.
bool NodeExtraMethod(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  State* s=detail::Self(args);
  const int32_t op=WebKit::Slot(args);
  JS::RootedObject self(cx);
  if(op==8) {
    if(!detail::NodeReceiver(cx,args,kOscillatorNode,&self)) return false;
  } else if(!FieldReceiver(cx,args,op<4?1:op==4?2:op==5?8:9,&self)) return false;
  if(op<4) {
    if(!args.get(0).isObject()) return WebKit::Fail(cx,"Analyser requires a typed array");
    JS::RootedObject data(cx,&args[0].toObject());
    if(op<2 ? !JS_IsUint8Array(data) : !JS_IsFloat32Array(data))
      return WebKit::Fail(cx,"Analyser data has an incompatible typed array");
    JS::RootedValue fft(cx),minimum(cx);
    if(!ReadField(cx,self,"fftSize",&fft) || !ReadField(cx,self,"minDecibels",&minimum))
      return false;
    const size_t limit=size_t(fft.toNumber())/((op==0 || op==2)?2:1);
    const size_t count=std::min(JS_GetTypedArrayLength(data),limit);
    JS::RootedValue value(cx,JS::DoubleValue(op==1?128:op==2?minimum.toNumber():0));
    for(size_t i=0;i<count;++i)
      if(!JS_SetElement(cx,data,uint32_t(i),value)) return false;
  } else if(op==6 || op==7) {
    const char* position[]={"positionX","positionY","positionZ"};
    const char* orientation[]={"orientationX","orientationY","orientationZ"};
    for(int i=0;i<3;++i) {
      double value=0;
      JS::RootedValue param(cx);
      if(!JS::ToNumber(cx,args.get(i),&value) ||
         !ReadField(cx,self,op==6?position[i]:orientation[i],&param)) return false;
      if(!std::isfinite(value)) return WebKit::Fail(cx,"Panner coordinates must be finite");
      const uint32_t id=uint32_t(JS::GetReservedSlot(&param.toObject(),0).toInt32());
      if(!s->model.set_param_value(id,value)) return false;
    }
  }
  // Public ruyijs's filter response and setPeriodicWave leave state unchanged.
  args.rval().setUndefined();return true;
}
}
