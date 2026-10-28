// One conversion and controller factory; instances retain only traced JS edges.
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include <cmath>

namespace ruyijs::resource_network {
namespace {
const JSErrorFormatString* Format(void* data,unsigned) {return static_cast<const JSErrorFormatString*>(data);}
bool Callable(JSContext* cx,JS::HandleObject object,const char* name,const char* prefix,JS::MutableHandleValue value) {
  if (!object) {value.setUndefined();return true;}
  if (!JS_GetProperty(cx,object,name,value)) return false;
  if (value.isUndefined() || (value.isObject() && JS::IsCallable(&value.toObject()))) return true;
  JSErrorFormatString format{"RUYIJS_SINK_CALLBACK","{0}{1}",2,JSEXN_TYPEERR};
  JS_ReportErrorNumberASCII(cx,Format,&format,0,prefix,value.isObject()?" is not callable.":" is not an object.");
  return false;
}
}
bool SetupWritable(State& state,JS::HandleObject stream,JS::HandleValue sinkValue,JS::HandleValue strategyValue) {
  JSContext* cx=state.cx;
  if (!sinkValue.isUndefined() && !sinkValue.isObject())
    return Error(cx,"WritableStream constructor: Argument 1 is not an object.");
  if (!strategyValue.isNullOrUndefined() && !strategyValue.isObject())
    return Error(cx,"WritableStream constructor: Argument 2 can't be converted to a dictionary.");
  JS::RootedObject sink(cx,sinkValue.isObject()?&sinkValue.toObject():nullptr);
  JS::RootedObject strategy(cx,strategyValue.isObject()?&strategyValue.toObject():nullptr);
  JS::RootedValue hwm(cx),size(cx),abort(cx),close(cx),start(cx),type(cx),write(cx);
  double highWaterMark=1;
  if (strategy && !JS_GetProperty(cx,strategy,"highWaterMark",&hwm)) return false;
  if (!hwm.isUndefined() && !JS::ToNumber(cx,hwm,&highWaterMark)) return false;
  if (!Callable(cx,strategy,"size","WritableStream constructor: 'size' member of QueuingStrategy",&size) ||
      !Callable(cx,sink,"abort","WritableStream.constructor: 'abort' member of UnderlyingSink",&abort) ||
      !Callable(cx,sink,"close","WritableStream.constructor: 'close' member of UnderlyingSink",&close) ||
      !Callable(cx,sink,"start","WritableStream.constructor: 'start' member of UnderlyingSink",&start)) return false;
  if (sink && !JS_GetProperty(cx,sink,"type",&type)) return false;
  if (!Callable(cx,sink,"write","WritableStream.constructor: 'write' member of UnderlyingSink",&write)) return false;
  if (!type.isUndefined()) return Error(cx,"WritableStream constructor: Implementation preserved member 'type'",true);
  if (std::isnan(highWaterMark) || highWaterMark<0) return Error(cx,"WritableStream constructor: Invalid highWaterMark",true);
  return InitializeWritableController(state,stream,sinkValue,start,write,close,abort,size,highWaterMark);
}
}
