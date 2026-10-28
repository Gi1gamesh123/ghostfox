#pragma once
#include "resource_network/readable_stream.h"
#include "resource_network/callback_entry.h"

namespace ruyijs::resource_network {
enum DefaultControllerSlot { DStream, DSource, DStart, DPull, DCancel, DSize,
                             DHwm, DTotal, DHead, DTail, DFlags, DSlots };
enum DefaultControllerFlag { DStarted=1, DCloseRequested=2, DPulling=4, DPullAgain=8 };
enum ReadChunkSlot { ReadChunkValue, ReadChunkSize, ReadChunkNext, ReadChunkSlots };
extern const JSClass kDefaultControllerClass, kReadChunkClass;
inline int DefaultFlags(JSObject* controller) {
  return JS::GetReservedSlot(controller,DFlags).toInt32();
}
inline void SetDefaultFlags(JSObject* controller,int flags) {
  JS::SetReservedSlot(controller,DFlags,JS::Int32Value(flags));
}
JSObject* DefaultControllerReceiver(JS::HandleValue);
double DefaultDesiredSize(JSObject*);
void ResetDefaultQueue(JSObject*);
void ClearDefaultAlgorithms(JSObject*);
bool InstallDefaultController(State&);
bool SetupDefaultReadable(State&,JS::HandleObject,JS::HandleValue,JS::HandleValue);
bool InitializeReadableController(State&,JS::HandleObject stream,JS::HandleValue source,
    JS::HandleValue start,JS::HandleValue pull,JS::HandleValue cancel,JS::HandleValue size,
    double highWaterMark,bool bytes,uint64_t allocation);
bool StartDefaultReadable(JSContext*,JS::HandleObject);
CallbackResult InvokeDefaultCallback(JSContext*,JS::HandleObject,unsigned,JS::HandleObject methodGlobal,
    JS::HandleValue,JS::MutableHandleValue);
CallbackResult DefaultCallbackPromise(JSContext*,JS::HandleObject,unsigned,JS::HandleObject methodGlobal,
    JS::HandleValue,JS::MutableHandleObject);
bool DefaultPullIfNeeded(JSContext*,JS::HandleObject);
bool DefaultRead(JSContext*,JS::HandleObject stream,JS::HandleObject promise);
bool DefaultEnqueue(JSContext*,JS::HandleObject controller,JS::HandleValue chunk);
bool DefaultClose(JSContext*,JS::HandleObject controller);
bool DefaultError(JSContext*,JS::HandleObject controller,JS::HandleValue reason);
CallbackResult DefaultCancel(JSContext*,JS::HandleObject stream,JS::HandleValue reason,JS::MutableHandleValue,
    JS::HandleObject methodGlobal);
}
