// Writer Promise factory realm stays independent of stream and prototype realms.
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "runtime/native_constructor_trace.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
bool NewWritableWriter(JSContext* cx,JS::HandleObject stream,JS::HandleObject prototype,JS::MutableHandleObject result) {
  if (SlotObject(stream,WWriter)) return Error(cx,"WritableStream is already locked!");
  JS::RootedObject writer(cx,JS_NewObjectWithGivenProto(cx,&kWriterClass,prototype));
  JS::RootedObject ready(cx,JS::NewPromiseObject(cx,nullptr)),closed(cx,JS::NewPromiseObject(cx,nullptr));
  if (!writer || !ready || !closed) return false;
  JS::SetReservedSlot(writer,VReady,JS::ObjectValue(*ready));
  JS::SetReservedSlot(writer,VClosed,JS::ObjectValue(*closed));
  const auto status=WritableStatus(stream);
  JS::RootedValue error(cx,JS::GetReservedSlot(stream,WError));
  if (status==WriteStatus::Erroring || status==WriteStatus::Errored) {
    if (!WritablePromise(cx,ready,true,error,true)) return false;
    if (status==WriteStatus::Errored && !WritablePromise(cx,closed,true,error,true)) return false;
  } else {
    if ((status==WriteStatus::Closed || !JS::GetReservedSlot(stream,WPressure).toBoolean() || WritableClosing(stream)) &&
        !WritablePromise(cx,ready,false,JS::UndefinedHandleValue)) return false;
    if (status==WriteStatus::Closed && !WritablePromise(cx,closed,false,JS::UndefinedHandleValue)) return false;
  }
  JS::RootedObject wrappedStream(cx,stream),wrappedWriter(cx,writer);
  if (!JS_WrapObject(cx,&wrappedStream)) return false;
  {JSAutoRealm realm(cx,stream);if (!JS_WrapObject(cx,&wrappedWriter)) return false;}
  JS::SetReservedSlot(writer,VStream,JS::ObjectValue(*wrappedStream));
  JS::SetReservedSlot(stream,WWriter,JS::ObjectValue(*wrappedWriter));result.set(writer);return true;
}
bool WritableWriterConstructor(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
  if (!args.isConstructing()) return Error(cx,"WritableStreamDefaultWriter constructor: 'new' is required");
  JS::RootedObject prototype(cx);
  if (!WritableDesiredPrototype(cx,args,1,&prototype)) return false;
  if (!argc) return Error(cx,"WritableStreamDefaultWriter constructor: At least 1 argument required, but only 0 passed");
  JS::RootedObject stream(cx,WritableReceiver(args.get(0))),writer(cx);
  if (!stream) return Error(cx,"WritableStreamDefaultWriter constructor: Argument 1 does not implement interface WritableStream.");
  if (SlotObject(stream,WWriter)) return Error(cx,"WritableStreamDefaultWriter constructor: WritableStream is already locked!");
  if (!NewWritableWriter(cx,stream,prototype,&writer)) return false;
  args.rval().setObject(*writer);return trace.Finish(true);
}
bool WritableRelease(JSContext* cx,JS::HandleObject writer) {
  JS::RootedObject stream(cx,SlotObject(writer,VStream));if (!stream) return true;
  Error(cx,"Releasing lock");JS::RootedValue error(cx);
  if (!JS_GetPendingException(cx,&error)) return false;
  JS_ClearPendingException(cx);
  if (!WriterReject(cx,writer,VReady,error) || !WriterReject(cx,writer,VClosed,error)) return false;
  JS::SetReservedSlot(stream,WWriter,JS::UndefinedValue());
  JS::SetReservedSlot(writer,VStream,JS::UndefinedValue());return true;
}
}
