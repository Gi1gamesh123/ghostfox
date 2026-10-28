// Internal streams use the original converted-input factories and controllers.
#include "resource_network/transform_internal.h"
#include "resource_network/state.h"
#include "runtime/native_constructor_trace.h"
#include "js/Promise.h"

namespace ruyijs::resource_network {
namespace {
const JSClass kNativeOperation={"Resource transform native operation",JSCLASS_HAS_RESERVED_SLOTS(TransformNativeSlotCount)};
}
bool InitializeTransform(State& state,JS::HandleObject stream,JS::HandleValue transformer,
    JS::HandleValue transform,JS::HandleValue flush,JS::HandleValue writableSize,
    JS::HandleValue readableSize,double writableHwm,double readableHwm,JS::MutableHandleObject startResult) {
  JSContext* cx=state.cx;
  JS::RootedObject prototype(cx,state.transformController.prototype);
  JS::RootedObject controller(cx,JS_NewObjectWithGivenProto(cx,&kTransformControllerClass,prototype));
  JS::RootedObject start(cx,JS::NewPromiseObject(cx,nullptr)),change(cx,JS::NewPromiseObject(cx,nullptr));
  JS::RootedObject operation(cx,JS_NewObjectWithGivenProto(cx,&kNativeOperation,nullptr));
  JS::RootedObject sink(cx),source(cx),writable(cx),readable(cx),holder(cx),readerProto(cx,state.defaultReader.prototype);
  if (!controller || !start || !change || !operation) return false;
  JS::SetReservedSlot(operation,TNStream,JS::ObjectValue(*stream));
  JS::SetReservedSlot(operation,TNStart,JS::ObjectValue(*start));
  if (!NewNativeSinkAlgorithms(cx,operation,kTransformSinkSteps,&sink) ||
      !NewNativeSourceAlgorithms(cx,operation,kTransformSourceSteps,&source)) return false;
  JS::SetReservedSlot(stream,TController,JS::ObjectValue(*controller));
  JS::SetReservedSlot(stream,TChange,JS::ObjectValue(*change));
  JS::SetReservedSlot(stream,TPressure,JS::BooleanValue(true));
  JS::SetReservedSlot(controller,CTransform,JS::ObjectValue(*stream));
  JS::SetReservedSlot(controller,CTransformer,transformer);
  JS::SetReservedSlot(controller,CTransformCall,transform);
  JS::SetReservedSlot(controller,CFlushCall,flush);
  prototype=state.writableStream.prototype;
  if (!NewWritableStream(state,prototype,&writable)) return false;
  JS::SetReservedSlot(stream,TWritable,JS::ObjectValue(*writable));
  JS::RootedValue sinkValue(cx,JS::ObjectValue(*sink)),sourceValue(cx,JS::ObjectValue(*source));
  if (!InitializeWritableController(state,writable,sinkValue,JS::UndefinedHandleValue,
      JS::UndefinedHandleValue,JS::UndefinedHandleValue,JS::UndefinedHandleValue,writableSize,writableHwm)) return false;
  prototype=state.stream.prototype;
  if (!NewReadableStream(cx,prototype,readerProto,holder,&readable)) return false;
  JS::SetReservedSlot(readable,SByobPrototype,JS::ObjectValue(*state.byobReader.prototype));
  JS::SetReservedSlot(stream,TReadable,JS::ObjectValue(*readable));
  if (!InitializeReadableController(state,readable,sourceValue,JS::UndefinedHandleValue,
      JS::UndefinedHandleValue,JS::UndefinedHandleValue,readableSize,readableHwm,false,0)) return false;
  startResult.set(start);return true;
}
bool TransformConstructor(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
  if (!args.isConstructing()) return Error(cx,"TransformStream constructor: 'new' is required");
  auto& state=*FunctionOwner(args);
  JS::RootedValue transformer(cx),start(cx),transform(cx),flush(cx),writableSize(cx),readableSize(cx);
  double writableHwm=1,readableHwm=0;
  if (!ConvertTransformer(cx,args,&transformer,&start,&transform,&flush,&writableSize,&readableSize,&writableHwm,&readableHwm)) return false;
  JS::RootedObject prototype(cx);
  if (!TransformDesiredPrototype(cx,args,&prototype)) return false;
  JS::RootedObject stream(cx,JS_NewObjectWithGivenProto(cx,&kTransformClass,prototype)),promise(cx);
  if (!stream || !InitializeTransform(state,stream,transformer,transform,flush,writableSize,readableSize,writableHwm,readableHwm,&promise)) return false;
  JS::RootedObject controller(cx,SlotObject(stream,TController));
  JS::RootedValue value(cx);bool entered=false;
  if (TransformCall(cx,controller,start,JS::UndefinedHandleValue,false,&value,
      {"TransformStream constructor: Refusing to execute function from global in which script is disabled.",&state},&entered)!=CallbackResult::Success ||
      !JS::SafeResolve(cx,promise,value)) return false;
  args.rval().setObject(*stream);return trace.Finish(true);
}
}
