#pragma once
#include "resource_network/callback_entry.h"

namespace ruyijs::resource_network {
using NativeSinkStep=CallbackResult (*)(JSContext*,JS::HandleObject operation,
    JS::HandleObject controller,JS::HandleValue,JS::MutableHandleValue,CallbackContext);
struct NativeSinkSteps { NativeSinkStep start,write,close,abort; };
bool NewNativeSinkAlgorithms(JSContext*,JS::HandleObject,const NativeSinkSteps&,JS::MutableHandleObject);
bool HasNativeSinkAlgorithms(JSObject*);
CallbackResult InvokeNativeSink(JSContext*,JS::HandleObject,unsigned,JS::HandleValue,
    JS::MutableHandleValue,CallbackContext);
}
