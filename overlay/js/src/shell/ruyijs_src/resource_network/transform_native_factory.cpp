// Intrinsic factory for codecs: the original Transform implementation only.
#include "resource_network/native_transform.h"
#include "resource_network/transform_internal.h"
#include "resource_network/state.h"
#include "runtime/page_reference.h"
#include "jsfriendapi.h"
#include "js/GlobalObject.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
State* FactoryState(JSContext* cx,JS::HandleObject factory) {
  JS::RootedObject original(cx,js::CheckedUnwrapStatic(factory));
  if (!original || !JS_IsNativeFunction(original,TransformConstructor)) {
    Error(cx,"Transform factory owner is unavailable");return nullptr;
  }
  const auto owner=js::GetFunctionNativeReserved(original,0);
  const auto kind=js::GetFunctionNativeReserved(original,1);
  if (!owner.isDouble() || !owner.toPrivate() || !kind.isInt32() || kind.toInt32()!=0) {
    Error(cx,"Transform factory owner is invalid");return nullptr;
  }
  auto* state=static_cast<State*>(owner.toPrivate());
  if (state->cx!=cx || state->transformStream.constructor!=original) {
    Error(cx,"Transform factory identity changed");return nullptr;
  }
  return state;
}
}
bool NativeTransformFactory(JSContext* cx,JS::HandleObject source,JS::MutableHandleObject result) {
  JS::RootedObject local(cx,js::CheckedUnwrapStatic(source));
  if (!local) return Error(cx,"Transform factory source is unavailable");
  JS::RootedObject global(cx,JS::GetNonCCWObjectGlobal(local));
  JS::RootedValue value(cx);
  if (!ReadPageReference(global,PageReference::ResourceTransformConstructor,&value) || !value.isObject())
    return Error(cx,"Transform factory realm is not installed");
  result.set(&value.toObject());
  return FactoryState(cx,result) && JS_WrapObject(cx,result);
}
bool NewNativeTransform(JSContext* cx,JS::HandleObject factory,JS::HandleObject operation,
    const NativeTransformSteps& steps,JS::MutableHandleObject transform,
    JS::MutableHandleObject readable,JS::MutableHandleObject writable) {
  State* state=FactoryState(cx,factory);
  if (!state) return false;
  JS::RootedObject stream(cx);
  {
    JSAutoRealm realm(cx,state->page);
    JS::RootedObject record(cx),prototype(cx,state->transformStream.prototype),start(cx);
    if (!NewNativeTransformer(cx,operation,steps,&record)) return false;
    stream=JS_NewObjectWithGivenProto(cx,&kTransformClass,prototype);
    JS::RootedValue native(cx,JS::ObjectValue(*record));
    if (!stream) return false;
    if (!InitializeTransform(*state,stream,native,JS::UndefinedHandleValue,
        JS::UndefinedHandleValue,JS::UndefinedHandleValue,JS::UndefinedHandleValue,1,0,&start)) {
      // The original full cleanup requires both streams; partial setup does not.
      if (JSObject* controller=SlotObject(stream,TController)) ClearTransformAlgorithms(controller);
      return false;
    }
    if (!JS::ResolvePromise(cx,start,JS::UndefinedHandleValue)) return TransformNativeFailure(cx,stream);
    transform.set(stream);
    readable.set(SlotObject(stream,TReadable));writable.set(SlotObject(stream,TWritable));
  }
  return (JS_WrapObject(cx,transform) && JS_WrapObject(cx,readable) && JS_WrapObject(cx,writable)) ||
      TransformNativeFailure(cx,stream);
}
}
