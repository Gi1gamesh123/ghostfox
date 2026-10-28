#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
bool WritableLocked(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject stream(cx,WritableReceiver(args.thisv()));
  if (!stream) return Error(cx,"'get locked' called on an object that does not implement interface WritableStream.");
  args.rval().setBoolean(SlotObject(stream,WWriter)!=nullptr);return true;
}
bool WritableAbortCall(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject stream(cx,WritableReceiver(args.thisv()));
  if (!stream) return TypeErrorPromise(cx,"'abort' called on an object that does not implement interface WritableStream.",args.rval());
  if (SlotObject(stream,WWriter)) return WritableTypeRejection(cx,stream,"Canceled Locked Stream",args.rval());
  return WritableAbort(cx,stream,args.get(0),args.rval(),
      {"WritableStream.abort: Refusing to execute function from global in which script is disabled.",FunctionOwner(args)}) || WritableExceptionRejection(cx,args.rval());
}
bool WritableCloseCall(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject stream(cx,WritableReceiver(args.thisv()));
  if (!stream) return TypeErrorPromise(cx,"'close' called on an object that does not implement interface WritableStream.",args.rval());
  if (SlotObject(stream,WWriter)) return WritableTypeRejection(cx,stream,"Can not close locked stream",args.rval());
  if (WritableClosing(stream)) return WritableTypeRejection(cx,stream,"Stream is already closing",args.rval());
  return WritableClose(cx,stream,args.rval(),
      {"WritableStream.close: Refusing to execute function from global in which script is disabled.",FunctionOwner(args)}) || WritableExceptionRejection(cx,args.rval());
}
bool WritableGetWriter(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject stream(cx,WritableReceiver(args.thisv())),writer(cx);
  if (!stream) return Error(cx,"'getWriter' called on an object that does not implement interface WritableStream.");
  if (SlotObject(stream,WWriter)) return Error(cx,"WritableStream.getWriter: WritableStream is already locked!");
  {
    JSAutoRealm realm(cx,stream);JS::RootedObject prototype(cx,SlotObject(stream,WWriterPrototype));
    if (!NewWritableWriter(cx,stream,prototype,&writer)) return false;
  }
  args.rval().setObject(*writer);return JS_WrapValue(cx,args.rval());
}
}
