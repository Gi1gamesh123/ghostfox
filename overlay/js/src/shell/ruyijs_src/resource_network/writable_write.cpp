// Size can reenter release/error/close; recheck the original writer after it.
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Realm.h"
#include <cmath>

namespace ruyijs::resource_network {
namespace {
bool CaptureError(JSContext* cx,JS::MutableHandleValue error) {
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,error)) return false;
  JS_ClearPendingException(cx);return true;
}
bool ChunkSize(JSContext* cx,JS::HandleObject controller,JS::HandleValue chunk,double* size,State* exceptionOwner) {
  JSAutoRealm realm(cx,controller);
  JS::RootedValue callback(cx,JS::GetReservedSlot(controller,KSize)),value(cx),input(cx,chunk);
  *size=1;if (callback.isUndefined()) return true;
  const auto entry=SinkEntry(cx,controller,callback,
      {"WritableStreamDefaultController.[[strategySizeAlgorithm]]: Refusing to execute function from global in which script is disabled.",exceptionOwner});
  if (entry==CallbackResult::Failure) return false;
  if (entry==CallbackResult::Success && JS_WrapValue(cx,&callback) && JS_WrapValue(cx,&input) &&
      JS::Call(cx,JS::UndefinedHandleValue,callback,JS::HandleValueArray(input),&value) &&
      JS::ToNumber(cx,value,size)) return true;
  JS::RootedValue error(cx);
  if (!CaptureError(cx,&error) || !WritableControllerError(cx,controller,error)) return false;
  *size=1;return true;
}
}
bool WritableWrite(JSContext* cx,JS::HandleObject writer,JS::HandleValue chunk,JS::MutableHandleValue result,State* exceptionOwner) {
  JS::RootedObject stream(cx,SlotObject(writer,VStream));
  if (!stream) return TypeErrorPromise(cx,"WritableStreamDefaultWriter.write: Missing stream",result);
  JS::RootedObject controller(cx,SlotObject(stream,WController));double size=1;
  if (!ChunkSize(cx,controller,chunk,&size,exceptionOwner)) return false;
  if (SlotObject(writer,VStream)!=stream)
    return TypeErrorPromise(cx,"WritableStreamDefaultWriter.write: Can not write on WritableStream owned by another writer.",result);
  const auto status=WritableStatus(stream);
  if (status==WriteStatus::Errored || status==WriteStatus::Erroring) {
    {JSAutoRealm realm(cx,writer);JS::RootedValue reason(cx,JS::GetReservedSlot(stream,WError));
      if (!JS_WrapValue(cx,&reason) || !Rejected(cx,reason,result)) return false;}
    return JS_WrapValue(cx,result);
  }
  if (WritableClosing(stream) || status==WriteStatus::Closed)
    return WritableTypeRejection(cx,writer,"Stream is closed or closing",result);
  JS::RootedObject request(cx),promise(cx),entry(cx);
  if (!NewWriteRequest(cx,stream,&request,&promise)) return false;
  if (std::isnan(size) || size<0 || std::isinf(size)) {
    Error(cx,std::isinf(size)&&size>0?"WritableStreamDefaultController.write: Infinite queue size":
          "WritableStreamDefaultController.write: invalid size",true);
    JS::RootedValue error(cx);
    if (!CaptureError(cx,&error)) return false;
    // This request joins the FIFO before size validation errors the controller;
    // pending start/in-flight write still gate its rejection like every write.
    AppendWriteRequest(stream,request);
    if (!WritableControllerError(cx,controller,error)) return false;
  } else {
    if (!NewWriteEntry(cx,controller,chunk,size,false,&entry)) return false;
    // Allocate a replacement ready before publishing the paired FIFO links.
    const double total=JS::GetReservedSlot(controller,KTotal).toNumber()+size;
    const double desired=JS::GetReservedSlot(controller,KHwm).toNumber()-total;
    if (!WritablePressure(cx,stream,desired<=0)) return false;
    AppendWriteRequest(stream,request);AppendWriteEntry(controller,entry);
    if (WritableAdvance(cx,controller,
        {"WritableStreamDefaultWriter.write: Refusing to execute function from global in which script is disabled.",exceptionOwner})!=CallbackResult::Success) return false;
  }
  result.setObject(*promise);return JS_WrapValue(cx,result);
}
}
