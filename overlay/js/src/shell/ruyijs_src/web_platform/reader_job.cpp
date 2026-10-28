// The FileReader job. One promise job of the engine job queue carries the computed
// value (or error) plus the generation; when it runs it applies the result and
// fires progress/load|error/loadend on the interaction tree. A superseded read is
// dropped without firing anything, so onload/onerror never run synchronously and
// never run twice.
#include "web_platform/state.h"

#include "interaction/realm.h"
#include "runtime/web_kit.h"
#include "jsfriendapi.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_platform {
namespace {
enum JobIndex { kJobReader = 0, kJobGeneration, kJobFailed, kJobPayload, kJobCount };

bool JobAction(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedValue info(cx, js::GetFunctionNativeReserved(&args.callee(), 1));
  JS::RootedObject info_object(cx);
  JS::RootedValue reader_value(cx), generation(cx), failed(cx), payload(cx);
  if (!info.isObject()) return false;
  info_object = &info.toObject();
  if (!JS_GetElement(cx, info_object, kJobReader, &reader_value) ||
      !reader_value.isObject() ||
      !JS_GetElement(cx, info_object, kJobGeneration, &generation) ||
      !JS_GetElement(cx, info_object, kJobFailed, &failed) ||
      !JS_GetElement(cx, info_object, kJobPayload, &payload))
    return false;
  JS::RootedObject reader(cx, &reader_value.toObject());
  if (JS::GetReservedSlot(reader, kReaderGenerationSlot).toInt32() != generation.toInt32())
    return true;  // A later read or abort superseded this one.
  if (JS::GetReservedSlot(reader, kReaderStateSlot).toInt32() != kReaderReadyLoading)
    return true;  // Nothing left to deliver.
  const bool error = failed.toBoolean();
  if (error) {
    JS::SetReservedSlot(reader, kReaderResultSlot, JS::NullValue());
    JS::SetReservedSlot(reader, kReaderErrorSlot, payload);
  } else {
    JS::SetReservedSlot(reader, kReaderResultSlot, payload);
    JS::SetReservedSlot(reader, kReaderErrorSlot, JS::NullValue());
  }
  JS::SetReservedSlot(reader, kReaderStateSlot, JS::Int32Value(kReaderReadyDone));
  return state.events->Fire(reader, "progress", false, false) &&
         state.events->Fire(reader, error ? "error" : "load", false, false) &&
         state.events->Fire(reader, "loadend", false, false);
}
}  // namespace

bool QueueReaderJob(State& state, JS::HandleObject reader, int32_t generation,
                    bool failed, JS::HandleValue payload) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject action(cx, state.kit.Function(JobAction, "FileReader task", 0, 0));
  JS::RootedObject info(cx, JS::NewArrayObject(cx, kJobCount));
  JS::RootedObject trigger(cx, JS::NewPromiseObject(cx, nullptr));
  if (!action || !info || !trigger) return false;
  JS::RootedValue reader_value(cx, JS::ObjectValue(*reader));
  JS::RootedValue generation_value(cx, JS::Int32Value(generation));
  JS::RootedValue failed_value(cx, JS::BooleanValue(failed));
  if (!JS_SetElement(cx, info, kJobReader, reader_value) ||
      !JS_SetElement(cx, info, kJobGeneration, generation_value) ||
      !JS_SetElement(cx, info, kJobFailed, failed_value) ||
      !JS_SetElement(cx, info, kJobPayload, payload))
    return false;
  js::SetFunctionNativeReserved(action, 1, JS::ObjectValue(*info));
  JS::RootedObject handler(cx, action);
  return JS::AddPromiseReactions(cx, trigger, handler, handler) &&
         JS::ResolvePromise(cx, trigger, JS::UndefinedHandleValue);
}
}  // namespace ruyijs::web_platform
