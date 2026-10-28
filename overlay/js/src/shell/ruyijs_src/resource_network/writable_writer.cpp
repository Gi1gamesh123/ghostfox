// Binding validation uses the method realm; valid requests delegate to one state.
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
bool Abort(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject writer(cx,WriterReceiver(args.thisv()));
  if (!writer) return TypeErrorPromise(cx,"'abort' called on an object that does not implement interface WritableStreamDefaultWriter.",args.rval());
  JS::RootedObject stream(cx,SlotObject(writer,VStream));
  if (!stream) return TypeErrorPromise(cx,"WritableStreamDefaultWriter.abort: Missing stream",args.rval());
  return WritableAbort(cx,stream,args.get(0),args.rval(),
      {"WritableStreamDefaultWriter.abort: Refusing to execute function from global in which script is disabled.",FunctionOwner(args)}) || WritableExceptionRejection(cx,args.rval());
}
bool Close(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject writer(cx,WriterReceiver(args.thisv()));
  if (!writer) return TypeErrorPromise(cx,"'close' called on an object that does not implement interface WritableStreamDefaultWriter.",args.rval());
  JS::RootedObject stream(cx,SlotObject(writer,VStream));
  if (!stream) return TypeErrorPromise(cx,"WritableStreamDefaultWriter.close: Missing stream",args.rval());
  if (WritableClosing(stream)) return TypeErrorPromise(cx,"WritableStreamDefaultWriter.close: Stream is closing",args.rval());
  return WritableClose(cx,stream,args.rval(),
      {"WritableStreamDefaultWriter.close: Refusing to execute function from global in which script is disabled.",FunctionOwner(args)}) || WritableExceptionRejection(cx,args.rval());
}
bool Write(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject writer(cx,WriterReceiver(args.thisv()));
  if (!writer) return TypeErrorPromise(cx,"'write' called on an object that does not implement interface WritableStreamDefaultWriter.",args.rval());
  return WritableWrite(cx,writer,args.get(0),args.rval(),FunctionOwner(args)) || WritableExceptionRejection(cx,args.rval());
}
bool Release(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject writer(cx,WriterReceiver(args.thisv()));
  if (!writer) return Error(cx,"'releaseLock' called on an object that does not implement interface WritableStreamDefaultWriter.");
  args.rval().setUndefined();return WritableRelease(cx,writer);
}
bool Closed(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject writer(cx,WriterReceiver(args.thisv()));
  if (!writer) return TypeErrorPromise(cx,"'get closed' called on an object that does not implement interface WritableStreamDefaultWriter.",args.rval());
  args.rval().set(JS::GetReservedSlot(writer,VClosed));return JS_WrapValue(cx,args.rval());
}
bool Ready(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject writer(cx,WriterReceiver(args.thisv()));
  if (!writer) return TypeErrorPromise(cx,"'get ready' called on an object that does not implement interface WritableStreamDefaultWriter.",args.rval());
  args.rval().set(JS::GetReservedSlot(writer,VReady));return JS_WrapValue(cx,args.rval());
}
bool Desired(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject writer(cx,WriterReceiver(args.thisv()));
  if (!writer) return Error(cx,"'get desiredSize' called on an object that does not implement interface WritableStreamDefaultWriter.");
  JS::RootedObject stream(cx,SlotObject(writer,VStream));
  if (!stream) return Error(cx,"WritableStreamDefaultWriter.desiredSize getter: Missing stream");
  const auto status=WritableStatus(stream);
  if (status==WriteStatus::Errored || status==WriteStatus::Erroring) args.rval().setNull();
  else args.rval().setDouble(status==WriteStatus::Closed?0:WritableDesiredSize(SlotObject(stream,WController)));
  return true;
}
}
bool InstallWritableWriter(State& state) {
  JS::RootedObject parent(state.cx,JS::GetRealmObjectPrototype(state.cx));auto& k=state.kit;auto& face=state.writableWriter;
  return parent && k.Define("WritableStreamDefaultWriter",&kWriterClass,parent,WritableWriterConstructor,&face,1) &&
      k.Method(face,"WritableStreamDefaultWriter","abort",Abort,0,0) &&
      k.Method(face,"WritableStreamDefaultWriter","close",Close,0,0) &&
      k.Method(face,"WritableStreamDefaultWriter","releaseLock",Release,0,0) &&
      k.Method(face,"WritableStreamDefaultWriter","write",Write,0,0) &&
      k.Accessor(face,"WritableStreamDefaultWriter","closed",Closed,nullptr,0) &&
      k.Accessor(face,"WritableStreamDefaultWriter","desiredSize",Desired,nullptr,0) &&
      k.Accessor(face,"WritableStreamDefaultWriter","ready",Ready,nullptr,0);
}
}
