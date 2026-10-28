#pragma once
#include "resource_network/readable_stream.h"
#include "resource_network/callback_entry.h"

namespace ruyijs::resource_network {
enum WritableSlot { WStatus,WError,WController,WWriter,WHead,WTail,WFlightWrite,
  WClose,WFlightClose,WAbort,WAbortReason,WAbortErroring,WPressure,WWriterPrototype,WNativeOwner,WSlots };
enum SinkSlot { KStream,KSink,KStart,KWrite,KClose,KAbort,KSize,KHwm,KTotal,KHead,KTail,KStarted,KSignal,KSlots };
enum WriterSlot { VStream,VReady,VClosed,VSlots };
enum WriteEntrySlot { EChunk,ESize,ENext,EClose,ESlots };
enum WriteRequestSlot { FPromise,FNext,FSlots };
enum class WriteStatus { Writable,Erroring,Errored,Closed };
enum class SinkReaction { Start,Write,Close,Abort };
extern const JSClass kWritableClass,kWriterClass,kSinkControllerClass,kWriteEntryClass,kWriteRequestClass;
inline WriteStatus WritableStatus(JSObject* s) {return WriteStatus(JS::GetReservedSlot(s,WStatus).toInt32());}
inline void SetWritableStatus(JSObject* s,WriteStatus state) {JS::SetReservedSlot(s,WStatus,JS::Int32Value(int(state)));}
inline bool WritableClosing(JSObject* s) {return SlotObject(s,WClose)||SlotObject(s,WFlightClose);}
inline bool WritableInFlight(JSObject* s) {return SlotObject(s,WFlightWrite)||SlotObject(s,WFlightClose);}
JSObject* WritableReceiver(JS::HandleValue);
JSObject* WriterReceiver(JS::HandleValue);
JSObject* SinkReceiver(JS::HandleValue);
bool WritableStore(JSContext*,JS::HandleObject,unsigned,JS::HandleValue);
bool WritableStoreObject(JSContext*,JS::HandleObject,unsigned,JS::HandleObject);
bool WritablePromise(JSContext*,JS::HandleObject,bool,JS::HandleValue,bool handled=false);
bool WriterReject(JSContext*,JS::HandleObject,unsigned,JS::HandleValue);
bool WritableTypeRejection(JSContext*,JS::HandleObject,const char*,JS::MutableHandleValue);
bool WritableExceptionRejection(JSContext*,JS::MutableHandleValue);
void ClearSinkAlgorithms(JSObject*);
void ResetWritableQueue(JSObject*);
double WritableDesiredSize(JSObject*);
bool WritablePressure(JSContext*,JS::HandleObject,bool);
CallbackResult WritableStartErroring(JSContext*,JS::HandleObject,JS::HandleValue,CallbackContext context={});
CallbackResult WritableFinishErroring(JSContext*,JS::HandleObject,CallbackContext context={});
CallbackResult WritableDealRejection(JSContext*,JS::HandleObject,JS::HandleValue,CallbackContext context={});
bool WritableRejectClosed(JSContext*,JS::HandleObject);
bool WritableControllerError(JSContext*,JS::HandleObject,JS::HandleValue);
bool WritableAbort(JSContext*,JS::HandleObject,JS::HandleValue,JS::MutableHandleValue,
    CallbackContext context={"WritableStreamDefaultWriter.abort: Refusing to execute function from global in which script is disabled."});
bool WritableClose(JSContext*,JS::HandleObject,JS::MutableHandleValue,
    CallbackContext context={"WritableStreamDefaultWriter.close: Refusing to execute function from global in which script is disabled."});
bool WritableWrite(JSContext*,JS::HandleObject,JS::HandleValue,JS::MutableHandleValue,State* exceptionOwner=nullptr);
bool WritableRelease(JSContext*,JS::HandleObject);
bool NewWritableWriter(JSContext*,JS::HandleObject,JS::HandleObject,JS::MutableHandleObject);
bool NewWriteEntry(JSContext*,JS::HandleObject,JS::HandleValue,double,bool,JS::MutableHandleObject);
void AppendWriteEntry(JSObject*,JSObject*);
void PopWriteEntry(JSObject*);
bool NewWriteRequest(JSContext*,JS::HandleObject,JS::MutableHandleObject,JS::MutableHandleObject);
void AppendWriteRequest(JSObject*,JSObject*);
JSObject* PopWriteRequest(JSObject*);
CallbackResult WritableAdvance(JSContext*,JS::HandleObject,CallbackContext context={});
CallbackResult SinkCall(JSContext*,JS::HandleObject,unsigned,JS::HandleValue,JS::MutableHandleValue,CallbackContext context={});
CallbackResult SinkPromise(JSContext*,JS::HandleObject,unsigned,JS::HandleValue,JS::MutableHandleObject,CallbackContext context={});
CallbackResult SinkEntry(JSContext*,JS::HandleObject,JS::HandleValue,CallbackContext);
bool ObserveSink(JSContext*,JS::HandleObject,JS::HandleObject,SinkReaction,JS::HandleObject);
bool WritableNativeFailure(JSContext*,JS::HandleObject);
CallbackResult SinkReact(JSContext*,JS::HandleObject,SinkReaction,bool,JS::HandleValue,JS::HandleObject);
bool SetupWritable(State&,JS::HandleObject,JS::HandleValue,JS::HandleValue);
bool InitializeWritableController(State&,JS::HandleObject stream,JS::HandleValue sink,
    JS::HandleValue start,JS::HandleValue write,JS::HandleValue close,JS::HandleValue abort,
    JS::HandleValue size,double highWaterMark);
bool NewWritableStream(State&,JS::HandleObject prototype,JS::MutableHandleObject);
bool WritableDesiredPrototype(JSContext*,const JS::CallArgs&,unsigned,JS::MutableHandleObject);
bool InstallWritableStream(State&,PageHelpers&);
bool InstallWritableWriter(State&);
bool InstallSinkController(State&);
bool WritableConstructor(JSContext*,unsigned,JS::Value*);
bool WritableWriterConstructor(JSContext*,unsigned,JS::Value*);
bool WritableLocked(JSContext*,unsigned,JS::Value*);
bool WritableAbortCall(JSContext*,unsigned,JS::Value*);
bool WritableCloseCall(JSContext*,unsigned,JS::Value*);
bool WritableGetWriter(JSContext*,unsigned,JS::Value*);
}
