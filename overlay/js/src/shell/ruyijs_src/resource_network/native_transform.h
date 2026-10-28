#pragma once
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs::resource_network {
// Synchronous native codecs use the original Transform algorithm/Promise path.
using NativeTransformStep=bool (*)(JSContext*,JS::HandleObject operation,
    JS::HandleObject controller,JS::HandleValue chunk);
// The record borrows this immutable table; callers must give it static lifetime.
struct NativeTransformSteps { NativeTransformStep transform,flush; };
bool NativeTransformFactory(JSContext*,JS::HandleObject source,JS::MutableHandleObject);
bool NewNativeTransform(JSContext*,JS::HandleObject factory,JS::HandleObject operation,
    const NativeTransformSteps&,JS::MutableHandleObject transform,
    JS::MutableHandleObject readable,JS::MutableHandleObject writable);
bool NativeTransformEnqueue(JSContext*,JS::HandleObject controller,JS::HandleValue chunk);
bool NewNativeTransformer(JSContext*,JS::HandleObject,const NativeTransformSteps&,JS::MutableHandleObject);
bool HasNativeTransformer(JSObject* controller);
bool InvokeNativeTransformer(JSContext*,JS::HandleObject controller,unsigned slot,JS::HandleValue chunk);
}
