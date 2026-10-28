#pragma once
#include "resource_network/readable_controller.h"
#include "js/ScalarType.h"
#include <cstdint>

namespace ruyijs::resource_network {
// R2's callback/flags prefix is shared; native records contain numeric data only.
enum ByteControllerSlot { BMeta=DSlots, BPendingHead, BPendingTail, BRequest,
                          BRequestPrototype, BSlots };
enum ByteQueueSlot { BQBuffer, BQNext, BQMeta, BQSlots };
enum PullIntoSlot { PBuffer, PNext, PMeta, PSlots };
enum ByobRequestSlot { YController, YView, YSlots };
enum class ReadKind { None, Default, Byob };
struct ByteState { uint64_t total=0,autoAllocate=0; };
struct ByteRegion { uint64_t offset=0,length=0; };
struct PullInto {
  uint64_t bufferLength=0,offset=0,capacity=0,filled=0,minimum=0,element=1;
  JS::Scalar::Type type=JS::Scalar::Uint8;
  ReadKind kind=ReadKind::Byob;
};
struct ByteView {
  uint64_t bufferLength=0,offset=0,length=0,element=1;
  JS::Scalar::Type type=JS::Scalar::Uint8;
};
extern const JSClass kByteControllerClass,kByteQueueClass,kPullIntoClass,
                     kByobReaderClass,kByobRequestClass;
ByteState& ByteData(JSObject*);
ByteRegion& RegionData(JSObject*);
PullInto& PullData(JSObject*);
bool IsByteStream(JSObject*);
bool IsByobReader(JSObject*);
JSObject* ByteControllerReceiver(JS::HandleValue);
JSObject* ByobReaderReceiver(JS::HandleValue);
JSObject* ByobRequestReceiver(JS::HandleValue);
bool InstallByteInterfaces(State&);
bool InstallByobReader(State&);
bool InstallByobRequest(State&);
bool NewByteController(JSContext*,JS::HandleObject stream,JS::HandleObject prototype,
                       JS::HandleObject requestPrototype,uint64_t,double,JS::MutableHandleObject);
bool NewByobReader(JSContext*,JS::HandleObject stream,JS::HandleObject prototype,JS::MutableHandleObject);
bool ConvertByteView(JSContext*,JS::HandleValue,ByteView*,JS::MutableHandleObject buffer,const char*);
bool EnforceU64(JSContext*,JS::HandleValue,uint64_t*,const char*);
bool TransferByteBuffer(JSContext*,JS::HandleObject,JS::MutableHandleObject);
bool NewByteView(JSContext*,JS::Scalar::Type,JS::HandleObject,uint64_t,uint64_t,JS::MutableHandleObject);
bool StoreByteBuffer(JSContext*,JS::HandleObject,unsigned,JS::HandleObject);
bool AppendByteRegion(JSContext*,JS::HandleObject,JS::HandleObject,uint64_t,uint64_t);
bool CopyByteRegion(JSContext*,JS::HandleObject,uint64_t,JS::HandleObject,uint64_t,uint64_t);
void ResetByteQueue(JSObject*);
double ByteDesiredSize(JSObject*);
bool NewPullInto(JSContext*,JS::HandleObject,JS::HandleObject,const PullInto&,JS::MutableHandleObject);
void AppendPullInto(JSObject*,JSObject*);
JSObject* ShiftPullInto(JSObject*);
void InvalidateByobRequest(JSObject*);
bool GetByobRequest(JSContext*,JS::HandleObject,JS::MutableHandleObject);
bool FillPullInto(JSContext*,JS::HandleObject,JS::HandleObject,bool*);
bool CommitPullInto(JSContext*,JS::HandleObject,JS::HandleObject,bool);
bool PullIntoView(JSContext*,JS::HandleObject,JS::MutableHandleObject);
bool CloneByteRegion(JSContext*,JS::HandleObject,JS::HandleObject,uint64_t,uint64_t);
bool DetachPullInto(JSContext*,JS::HandleObject);
bool ProcessPullIntoQueue(JSContext*,JS::HandleObject);
bool ProcessByteQueue(JSContext*,JS::HandleObject);
bool ByteQueueDrained(JSContext*,JS::HandleObject);
bool ByteRead(JSContext*,JS::HandleObject,JS::HandleObject,JS::HandleObject,const ByteView&,uint64_t);
bool ByteDefaultRead(JSContext*,JS::HandleObject,JS::HandleObject);
bool ByteEnqueue(JSContext*,JS::HandleObject,JS::HandleValue);
bool ByteRespond(JSContext*,JS::HandleObject,uint64_t);
bool ByteRespondNewView(JSContext*,JS::HandleObject,JS::HandleValue);
bool ByteRespondInternal(JSContext*,JS::HandleObject,uint64_t);
enum class ByteCloseKind { Public, Buffered, Intrinsic };
enum class ByteCloseOutcome { Success, IncompleteElement, Failure };
bool ByteClose(JSContext*,JS::HandleObject,ByteCloseKind=ByteCloseKind::Public,
    ByteCloseOutcome* = nullptr);
bool ByteError(JSContext*,JS::HandleObject,JS::HandleValue);
bool ByteCancelSteps(JSContext*,JS::HandleObject);
void ByteRelease(JSObject*);
bool ByteFailure(JSContext*,JS::HandleObject,bool rethrow=true);
bool ByteCommitFailure(JSContext*,JS::HandleObject);
bool ResolveByobRead(JSContext*,JS::HandleObject,bool,JS::HandleValue);
bool RejectPendingException(JSContext*,JS::MutableHandleValue);
}
