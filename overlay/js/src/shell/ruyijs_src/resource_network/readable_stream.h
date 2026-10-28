#pragma once
#include "resource_network/body.h"
#include "resource_network/callback_entry.h"
#include "resource_network/read_completion.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::resource_network {
// All object edges are ordinary traced slots. No asynchronous job owns State*.
enum HolderSlot { HRecord, HStream, HStreamPrototype, HReaderPrototype,
                  HBytePrototype, HByobPrototype, HRequestPrototype, HFactories, HNativeSource, HSlots };
enum StreamSlot { SHolder, SReader, SHead, STail, SFlags, SReaderPrototype,
                  SStatus, SError, SSource, SController, SDisturbed, SByobPrototype, SNativeOwner, SSlots };
enum ReaderSlot { RStream, RClosed, RSlots };
enum StreamFactorySlot { SFStream,SFDefaultReader,SFByobReader,SFDefaultController,
                         SFByteController,SFByobRequest,SFOwner,SFSlots };
enum ReadRequestSlot { QTarget, QNext, QSlots };
enum StreamFlag { Delivered = 1, Scheduled = 4, EndScheduled = 8 };
enum class StreamStatus { Readable, Closed, Errored };
enum class StreamSource { BufferedBody, Default, Byte };
extern const JSClass kBodyHolderClass, kReadableReaderClass, kReadRequestClass;
inline int Flags(JSObject* stream) { return JS::GetReservedSlot(stream,SFlags).toInt32(); }
inline void SetFlags(JSObject* stream,int flags) { JS::SetReservedSlot(stream,SFlags,JS::Int32Value(flags)); }
inline StreamStatus ReadableStatus(JSObject* stream) {
  return StreamStatus(JS::GetReservedSlot(stream,SStatus).toInt32());
}
inline void SetReadableStatus(JSObject* stream,StreamStatus status) {
  JS::SetReservedSlot(stream,SStatus,JS::Int32Value(int(status)));
}
inline StreamSource Source(JSObject* stream) {
  return StreamSource(JS::GetReservedSlot(stream,SSource).toInt32());
}
inline JSObject* SlotObject(JSObject* object,unsigned slot) {
  auto value=JS::GetReservedSlot(object,slot);
  return value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
}
bool StreamFactories(JSContext*,JS::HandleObject source,JS::MutableHandleObject);
State* StreamFactoryState(JSContext*,JS::HandleObject factories);
JSObject* StreamReceiver(JS::HandleValue);
JSObject* ReaderReceiver(JS::HandleValue);
BodyRecord* StreamBody(JSObject*);
void DisturbReadable(JSObject*);
bool NewReadableStream(JSContext*,JS::HandleObject prototype,JS::HandleObject readerPrototype,
                       JS::HandleObject holder,JS::MutableHandleObject);
bool NewDefaultReader(JSContext*,JS::HandleObject stream,JS::HandleObject prototype,
                      JS::MutableHandleObject);
bool NewReader(JSContext*,JS::HandleObject stream,JS::HandleObject prototype,bool byob,JS::MutableHandleObject);
bool StreamDesiredPrototype(JSContext*,const JS::CallArgs&,unsigned,JS::MutableHandleObject);
bool CancelReader(JSContext*,JS::HandleObject,JS::HandleValue,JS::MutableHandleValue);
bool ReadDefaultReader(State&,JS::HandleObject reader,JS::MutableHandleValue result);
bool ReadDefaultReaderInto(State&,JS::HandleObject reader,JS::HandleObject target);
bool ReleaseReader(JSContext*,JS::HandleObject);
bool EnsureBodyStream(JSContext*,JS::HandleObject holder,JS::MutableHandleObject);
bool QueueRead(JSContext*,JS::HandleObject stream,JS::HandleObject promise);
bool PrepareReadRequest(JSContext*,JS::HandleObject,JS::HandleObject,JS::MutableHandleObject);
void AppendReadRequest(JSObject*,JSObject*);
JSObject* PopReadRequest(JSObject* stream);
bool ScheduleRead(JSContext*,JS::HandleObject stream);
bool ScheduleEnd(State&,JS::HandleObject stream);
bool CloseReadable(JSContext*,JS::HandleObject stream);
CallbackResult CancelReadable(JSContext*,JS::HandleObject stream,JS::HandleValue reason,JS::MutableHandleValue result,
    JS::HandleObject methodGlobal);
bool RejectReads(JSContext*,JS::HandleObject stream,const char*);
bool RejectReads(JSContext*,JS::HandleObject stream,JS::HandleValue reason);
bool ErrorReadable(JSContext*,JS::HandleObject stream,JS::HandleValue reason);
bool ResolveRead(JSContext*,JS::HandleObject promise,bool done,JS::HandleValue value);
bool ReaderConstructor(JSContext*,unsigned,JS::Value*);
bool ReaderRead(JSContext*,unsigned,JS::Value*);
bool ReaderRelease(JSContext*,unsigned,JS::Value*);
bool ReaderCancel(JSContext*,unsigned,JS::Value*);
bool ReaderClosed(JSContext*,unsigned,JS::Value*);
bool StreamLocked(JSContext*,unsigned,JS::Value*);
bool StreamCancel(JSContext*,unsigned,JS::Value*);
}
