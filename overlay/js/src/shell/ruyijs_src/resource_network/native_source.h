#pragma once
#include "resource_network/callback_entry.h"

namespace ruyijs::resource_network {
using NativeSourceStep=CallbackResult (*)(JSContext*,JS::HandleObject operation,
    JS::HandleObject controller,JS::HandleObject methodGlobal,JS::HandleValue,
    JS::MutableHandleValue);
struct NativeSourceSteps { NativeSourceStep start,pull,cancel; };
// Tables are immutable static-lifetime values, never JS-visible registrations.
bool NewNativeSourceAlgorithms(JSContext*,JS::HandleObject operation,
    const NativeSourceSteps&,JS::MutableHandleObject);
bool HasNativeSourceAlgorithms(JSObject* controller);
CallbackResult InvokeNativeSource(JSContext*,JS::HandleObject controller,unsigned slot,
    JS::HandleObject methodGlobal,JS::HandleValue,JS::MutableHandleValue);
}
