// WebIDL conversion/callback capture for the original default-source factory.
#include "resource_network/readable_controller.h"
#include "resource_network/readable_byte.h"
#include "resource_network/native_source.h"
#include "resource_network/state.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include <cmath>

namespace ruyijs::resource_network {
namespace {
bool Callable(JSContext* cx,JS::HandleObject source,const char* key,JS::MutableHandleValue value) {
  if (!source) {value.setUndefined();return true;}
  if (!JS_GetProperty(cx,source,key,value)) return false;
  return value.isUndefined() || (value.isObject() && JS::IsCallable(&value.toObject())) ||
         Error(cx,"ReadableStream callback must be callable");
}
bool AllocationSize(JSContext* cx,JS::HandleObject source,uint64_t* allocation,bool* present) {
  if (!source) return true;
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx,source,"autoAllocateChunkSize",&value)) return false;
  if (value.isUndefined()) return true;
  *present=true;
  return EnforceU64(cx,value,allocation,"ReadableStream.constructor: 'autoAllocateChunkSize' member of UnderlyingSource");
}
}
bool SetupDefaultReadable(State& state,JS::HandleObject stream,
                          JS::HandleValue sourceValue,JS::HandleValue strategyValue) {
  JSContext* cx=state.cx;
  if (!sourceValue.isUndefined() && !sourceValue.isObject())
    return Error(cx,"ReadableStream underlying source must be an object");
  if (!strategyValue.isNullOrUndefined() && !strategyValue.isObject())
    return Error(cx,"ReadableStream strategy must be a dictionary");
  JS::RootedObject source(cx,sourceValue.isObject()?&sourceValue.toObject():nullptr);
  JS::RootedObject strategy(cx,strategyValue.isObject()?&strategyValue.toObject():nullptr);
  JS::RootedValue hwm(cx),size(cx),cancel(cx),pull(cx),start(cx),type(cx);
  double highWaterMark=1;
  uint64_t allocation=0;bool allocationPresent=false,bytes=false;
  if (strategy && !JS_GetProperty(cx,strategy,"highWaterMark",&hwm)) return false;
  if (!hwm.isUndefined() && !JS::ToNumber(cx,hwm,&highWaterMark)) return false;
  if (!Callable(cx,strategy,"size",&size) || !AllocationSize(cx,source,&allocation,&allocationPresent) ||
      !Callable(cx,source,"cancel",&cancel) || !Callable(cx,source,"pull",&pull) ||
      !Callable(cx,source,"start",&start)) return false;
  if (source && !JS_GetProperty(cx,source,"type",&type)) return false;
  if (!type.isUndefined()) {
    std::string text;
    if (!Convert(cx,type,&text)) return false;
    if (text!="bytes") return Error(cx,"ReadableStream type must be bytes");
    if (!size.isUndefined()) return Error(cx,"A byte stream does not accept a size strategy",true);
    bytes=true;
    if (hwm.isUndefined()) highWaterMark=0;
  }
  // Range validation follows source dictionary conversion in the donor.
  if (std::isnan(highWaterMark) || highWaterMark<0)
    return Error(cx,"highWaterMark must be nonnegative",true);
  if (bytes && allocationPresent && !allocation) return Error(cx,"ReadableStream constructor: autoAllocateChunkSize can not be zero.");
  return InitializeReadableController(state,stream,sourceValue,start,pull,cancel,size,
                                      highWaterMark,bytes,allocation);
}
CallbackResult InvokeDefaultCallback(JSContext* cx,JS::HandleObject controller,unsigned slot,
    JS::HandleObject methodGlobal,JS::HandleValue argument,JS::MutableHandleValue result) {
  if (HasNativeSourceAlgorithms(controller))
    return InvokeNativeSource(cx,controller,slot,methodGlobal,argument,result);
  JS::RootedValue callback(cx,JS::GetReservedSlot(controller,slot));
  if (callback.isUndefined()) {result.setUndefined();return CallbackResult::Success;}
  JS::RootedValue receiver(cx,JS::GetReservedSlot(controller,DSource));
  JS::RootedValue input(cx,argument);
  return CallbackOutcome(JS_WrapValue(cx,&callback) && JS_WrapValue(cx,&receiver) && JS_WrapValue(cx,&input) &&
         JS::Call(cx,receiver,callback,JS::HandleValueArray(input),result));
}
}
