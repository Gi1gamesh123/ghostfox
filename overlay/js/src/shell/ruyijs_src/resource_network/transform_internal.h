#pragma once
#include "resource_network/writable_stream.h"
#include "resource_network/readable_controller.h"
#include "resource_network/native_sink.h"
#include "resource_network/native_source.h"

namespace ruyijs::resource_network {
enum TransformSlot { TReadable,TWritable,TController,TPressure,TChange,TransformSlotCount };
enum TransformNativeSlot { TNStream,TNStart,TransformNativeSlotCount };
enum TransformControllerSlot { CTransform,CTransformer,CTransformCall,CFlushCall,TransformControllerSlotCount };
enum class TransformReaction { Pressure,Transform,Flush };
extern const JSClass kTransformClass,kTransformControllerClass;
extern const NativeSinkSteps kTransformSinkSteps;
extern const NativeSourceSteps kTransformSourceSteps;
JSObject* TransformReceiver(JS::HandleValue);
JSObject* TransformControllerReceiver(JS::HandleValue);
bool TransformDesiredPrototype(JSContext*,const JS::CallArgs&,JS::MutableHandleObject);
bool TransformConstructor(JSContext*,unsigned,JS::Value*);
bool InstallTransformController(State&);
bool ConvertTransformer(JSContext*,const JS::CallArgs&,JS::MutableHandleValue,JS::MutableHandleValue,
    JS::MutableHandleValue,JS::MutableHandleValue,JS::MutableHandleValue,JS::MutableHandleValue,double*,double*);
bool InitializeTransform(State&,JS::HandleObject,JS::HandleValue,JS::HandleValue,JS::HandleValue,
    JS::HandleValue,JS::HandleValue,double,double,JS::MutableHandleObject);
void ClearTransformAlgorithms(JSObject*);
bool SetTransformPressure(JSContext*,JS::HandleObject,bool);
bool TransformErrorWritable(JSContext*,JS::HandleObject,JS::HandleValue);
bool TransformError(JSContext*,JS::HandleObject,JS::HandleValue);
bool TransformEnqueue(JSContext*,JS::HandleObject,JS::HandleValue);
bool TransformTerminate(JSContext*,JS::HandleObject);
CallbackResult TransformCall(JSContext*,JS::HandleObject,JS::HandleValue,JS::HandleValue,
    bool,JS::MutableHandleValue,CallbackContext,bool*);
CallbackResult TransformCallbackPromise(JSContext*,JS::HandleObject,unsigned,JS::HandleValue,
    JS::MutableHandleObject,CallbackContext);
CallbackResult PerformTransform(JSContext*,JS::HandleObject,JS::HandleValue,JS::MutableHandleValue,CallbackContext);
bool TransformThen(JSContext*,JS::HandleObject,JS::HandleObject,TransformReaction,JS::HandleValue,JS::MutableHandleValue);
bool TransformNativeFailure(JSContext*,JS::HandleObject);
}
