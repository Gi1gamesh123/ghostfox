#pragma once
#include "resource_network/native_source.h"
#include "resource_network/native_sink.h"

namespace ruyijs::resource_network {
struct State;
// State is borrowed only during creation; controllers retain the original
// native algorithms and consumer operation. Null prototype selects intrinsics.
bool NewNativeReadableStream(State&,JS::HandleObject operation,const NativeSourceSteps&,
    bool bytes,double highWaterMark,JS::HandleObject prototype,
    JS::MutableHandleObject stream,JS::MutableHandleObject controller);
bool NewNativeWritableStream(State&,JS::HandleObject operation,const NativeSinkSteps&,
    double highWaterMark,JS::HandleObject prototype,
    JS::MutableHandleObject stream,JS::MutableHandleObject controller);
bool NativeReadableEnqueue(JSContext*,JS::HandleObject controller,JS::HandleValue);
bool NativeReadableClose(JSContext*,JS::HandleObject controller);
bool NativeReadableError(JSContext*,JS::HandleObject controller,JS::HandleValue);
bool NativeWritableError(JSContext*,JS::HandleObject controller,JS::HandleValue);
}
