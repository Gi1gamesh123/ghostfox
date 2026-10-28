// WebIDL argument conversion precedes the original Transform initialization.
#include "resource_network/transform_internal.h"
#include "resource_network/state.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/PropertyAndElement.h"
#include <cmath>

namespace ruyijs::resource_network {
namespace {
const JSErrorFormatString* ConversionError(void* data,unsigned) {
  return static_cast<const JSErrorFormatString*>(data);
}
bool Callback(JSContext* cx,JS::HandleObject object,const char* name,const char* dictionary,JS::MutableHandleValue result) {
  if (!object) {result.setUndefined();return true;}
  if (!JS_GetProperty(cx,object,name,result)) return false;
  if (result.isUndefined() || (result.isObject() && JS::IsCallable(&result.toObject()))) return true;
  JSErrorFormatString format={"RUYIJS_TRANSFORM_CALLBACK",
      "TransformStream.constructor: '{0}' member of {1} is {2}.",3,JSEXN_TYPEERR};
  JS_ReportErrorNumberASCII(cx,ConversionError,&format,0,name,dictionary,
      result.isObject()?"not callable":"not an object");
  return false;
}
bool Strategy(JSContext* cx,JS::HandleValue input,unsigned index,double fallback,
    JS::MutableHandleValue size,double* hwm) {
  *hwm=fallback;
  if (input.isNullOrUndefined()) {size.setUndefined();return true;}
  if (!input.isObject()) return Error(cx,index==2?
      "TransformStream constructor: Argument 2 can't be converted to a dictionary.":
      "TransformStream constructor: Argument 3 can't be converted to a dictionary.");
  JS::RootedObject strategy(cx,&input.toObject());JS::RootedValue value(cx);
  return JS_GetProperty(cx,strategy,"highWaterMark",&value) &&
      (value.isUndefined() || JS::ToNumber(cx,value,hwm)) && Callback(cx,strategy,"size","QueuingStrategy",size);
}
}
bool ConvertTransformer(JSContext* cx,const JS::CallArgs& args,JS::MutableHandleValue transformer,
    JS::MutableHandleValue start,JS::MutableHandleValue transform,JS::MutableHandleValue flush,
    JS::MutableHandleValue writableSize,JS::MutableHandleValue readableSize,double* writableHwm,double* readableHwm) {
  transformer.set(args.get(0));
  if (!transformer.isUndefined() && !transformer.isObject())
    return Error(cx,"TransformStream constructor: Argument 1 is not an object.");
  if (!Strategy(cx,args.get(1),2,1,writableSize,writableHwm) ||
      !Strategy(cx,args.get(2),3,0,readableSize,readableHwm)) return false;
  JS::RootedObject object(cx,transformer.isObject()?&transformer.toObject():nullptr);
  JS::RootedValue readableType(cx),writableType(cx);
  if (!Callback(cx,object,"flush","Transformer",flush) ||
      (object && !JS_GetProperty(cx,object,"readableType",&readableType)) ||
      !Callback(cx,object,"start","Transformer",start) ||
      !Callback(cx,object,"transform","Transformer",transform) ||
      (object && !JS_GetProperty(cx,object,"writableType",&writableType))) return false;
  if (!readableType.isUndefined())
    return Error(cx,"TransformStream constructor: `readableType` is unsupported and preserved for future use",true);
  if (!writableType.isUndefined())
    return Error(cx,"TransformStream constructor: `writableType` is unsupported and preserved for future use",true);
  if (std::isnan(*readableHwm) || *readableHwm<0 || std::isnan(*writableHwm) || *writableHwm<0)
    return Error(cx,"TransformStream constructor: Invalid highWaterMark",true);
  return true;
}
}
