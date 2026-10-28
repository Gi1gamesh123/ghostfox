// The page realm's own Blob constructor plus the one job queue this domain uses.
// A toBlob callback is delivered through an already resolved promise, so it runs
// on the engine job queue and never synchronously.
#include "web_canvas/internal.h"

#include "js/ArrayBuffer.h"
#include "js/Promise.h"
#include "js/Realm.h"
#include "jsfriendapi.h"
#include <cstring>

namespace ruyijs::web_canvas::detail {
namespace {

bool JobAction(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedValue info(cx, js::GetFunctionNativeReserved(&args.callee(), 1));
  if (!info.isObject()) return false;
  JS::RootedObject record(cx, &info.toObject());
  JS::RootedValue callback(cx);
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, record, 0, &callback) || !JS_GetElement(cx, record, 1, &value))
    return false;
  args.rval().setUndefined();
  JS::RootedValueArray<1> call(cx);
  call[0].set(value);
  JS::RootedValue ignored(cx);
  return JS::Call(cx, JS::UndefinedHandleValue, callback, call, &ignored);
}
}  // namespace

bool PageConstructBlob(State& state, const std::string& bytes, JS::HandleString type,
                       JS::MutableHandleObject out) {
  JSContext* cx = state.cx;
  JS::RootedObject buffer(cx, JS::NewArrayBuffer(cx, bytes.size()));
  if (!buffer) return false;
  if (!bytes.empty()) {
    bool shared = false;
    JS::AutoCheckCannotGC nogc;
    uint8_t* data = static_cast<uint8_t*>(
        JS::GetArrayBufferData(buffer, &shared, nogc));
    if (!data) return false;
    std::memcpy(data, bytes.data(), bytes.size());
  }
  JS::RootedObject options(cx, JS_NewPlainObject(cx));
  if (!options || !JS_DefineProperty(cx, options, "type", type, JSPROP_ENUMERATE))
    return false;
  JS::RootedObject parts(cx, JS::NewArrayObject(cx, 1));
  JS::RootedValue part(cx, JS::ObjectValue(*buffer));
  if (!parts || !JS_SetElement(cx, parts, 0, part)) return false;
  JS::RootedValueArray<2> construct(cx);
  construct[0].setObject(*parts);
  construct[1].setObject(*options);
  JS::RootedValue constructor(cx);
  if (!JS_GetProperty(cx, state.kit.page, "Blob", &constructor) || !constructor.isObject())
    return WebKit::Fail(cx, "canvas.toBlob requires the page Blob interface");
  return JS::Construct(cx, constructor, construct, out);
}

bool QueueJob(State& state, JS::HandleValue callback, JS::HandleValue value) {
  JSContext* cx = state.cx;
  const JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject action(cx, state.kit.Function(JobAction, "canvas task", 0, 0));
  JS::RootedObject record(cx, JS::NewArrayObject(cx, 2));
  JS::RootedObject trigger(cx, JS::NewPromiseObject(cx, nullptr));
  if (!action || !record || !trigger) return false;
  if (!JS_SetElement(cx, record, 0, callback) || !JS_SetElement(cx, record, 1, value))
    return false;
  js::SetFunctionNativeReserved(action, 1, JS::ObjectValue(*record));
  JS::RootedObject handler(cx, action);
  return JS::AddPromiseReactions(cx, trigger, handler, handler) &&
         JS::ResolvePromise(cx, trigger, JS::UndefinedHandleValue);
}
}  // namespace ruyijs::web_canvas::detail
