#include "web_audio/internal.h"

#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/ValueArray.h"
#include "js/Promise.h"
#include "jsfriendapi.h"

namespace ruyijs::web_audio {
using detail::IsContext;
using detail::Self;
namespace {
// The completion task, in Firefox's order (OfflineDestinationNode::
// FireOfflineCompletionEvent): the context becomes "closed", the promise
// resolves with the rendered buffer, then one trusted OfflineAudioCompletionEvent
// carries a buffer with the same contents (Firefox hands the event its own
// AudioBuffer object).
// The event task: the context and the event's buffer are in the record.
bool FireComplete(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject task(cx, &args.callee());  // before rval(), which shares the callee slot
  args.rval().setUndefined();
  State* state = static_cast<State*>(js::GetFunctionNativeReserved(task, 0).toPrivate());
  JS::RootedObject record(cx, &js::GetFunctionNativeReserved(task, 1).toObject());
  JS::RootedValue self(cx), event(cx);
  if (!JS_GetElement(cx, record, 0, &self) || !JS_GetElement(cx, record, 1, &event)) return false;
  JS::RootedObject context(cx, &self.toObject());
  return state->events->FireOfflineAudioComplete(context, event);
}
bool Complete(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject task(cx, &args.callee());  // before rval(), which shares the callee slot
  args.rval().setUndefined();
  State* state = static_cast<State*>(js::GetFunctionNativeReserved(task, 0).toPrivate());
  JS::RootedObject record(cx, &js::GetFunctionNativeReserved(task, 1).toObject());
  JS::RootedValue self(cx), buffer(cx), promise(cx);
  if (!JS_GetElement(cx, record, 0, &self) || !JS_GetElement(cx, record, 1, &buffer) ||
      !JS_GetElement(cx, record, 2, &promise)) return false;
  JS::RootedObject context(cx, &self.toObject()), pending(cx, &promise.toObject());
  JS::SetReservedSlot(context, kContextStateSlot, JS::Int32Value(kClosedState));
  if (!JS::ResolvePromise(cx, pending, buffer)) return false;
  JS::RootedObject rendered(cx, &buffer.toObject());
  JS::RootedObject copy(cx, NewBuffer(*state,
      JS::GetReservedSlot(context, kContextChannelsSlot).toInt32(),
      JS::GetReservedSlot(context, kContextLengthSlot).toInt32(),
      JS::GetReservedSlot(context, kContextRateSlot).toNumber()));
  if (!copy || !CopyBufferContents(*state, rendered, copy)) return false;
  // The event follows in its own task, after the promise's reactions ran
  // (Firefox: the resolution's microtasks run before the event is dispatched).
  JS::RootedObject next(cx, JS::NewArrayObject(cx, 2));
  JS::RootedFunction fn(cx, js::NewFunctionWithReserved(cx, FireComplete, 0, 0, "OfflineAudioContext complete"));
  if (!next || !fn) return false;
  JS::RootedValue event(cx, JS::ObjectValue(*copy));
  if (!JS_SetElement(cx, next, 0, self) || !JS_SetElement(cx, next, 1, event)) return false;
  JS::RootedObject fire(cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(fire, 0, JS::PrivateValue(state));
  js::SetFunctionNativeReserved(fire, 1, JS::ObjectValue(*next));
  if (state->taskPort && state->taskPort(state->taskContext, fire)) return true;
  return state->events->FireOfflineAudioComplete(context, event);
}
}  // namespace
// The offline render: a new AudioBuffer of the context's dimensions, filled
// by the renderer (D34). Rendering runs now; the completion event and the
// promise's resolution follow in a task of the one queue.
bool StartRendering(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = Self(args);
  if (!detail::IsContextKind(cx, args.thisv(), true)) return false;
  JS::RootedObject self(cx, &args.thisv().toObject());
  if (state->taskPort) {
    // A second startRendering() rejects (Firefox: InvalidStateError once rendering started).
    JS::RootedValue started(cx, JS::GetReservedSlot(self, kContextRenderingSlot));
    if (started.isTrue()) {
      JS::RootedValue reason(cx);
      if (!MakeDOMException(*state, "InvalidStateError",
                            "Cannot call startRendering more than once", &reason)) return false;
      return state->kit.RejectWith(reason, args.rval());
    }
    JS::SetReservedSlot(self, kContextRenderingSlot, JS::TrueValue());
  }
  JS::RootedObject buffer(cx, NewBuffer(*state,
      JS::GetReservedSlot(self, kContextChannelsSlot).toInt32(),
      JS::GetReservedSlot(self, kContextLengthSlot).toInt32(),
      JS::GetReservedSlot(self, kContextRateSlot).toNumber()));
  if (!buffer) return false;
  if (!RenderPlaceholder(*state, self, buffer)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*buffer));
  if (!state->taskPort) {  // no task queue bound: the previous synchronous order
    if (!state->events->FireOfflineAudioComplete(self, value)) return false;
    return state->kit.Resolve(value, args.rval());
  }
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  JS::RootedObject record(cx, JS::NewArrayObject(cx, 3));
  JS::RootedFunction fn(cx, js::NewFunctionWithReserved(cx, Complete, 0, 0, "OfflineAudioContext completion"));
  if (!promise || !record || !fn) return false;
  JS::RootedValue selfValue(cx, JS::ObjectValue(*self)), promiseValue(cx, JS::ObjectValue(*promise));
  if (!JS_SetElement(cx, record, 0, selfValue) || !JS_SetElement(cx, record, 1, value) ||
      !JS_SetElement(cx, record, 2, promiseValue)) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(state));
  js::SetFunctionNativeReserved(task, 1, JS::ObjectValue(*record));
  if (!state->taskPort(state->taskContext, task)) return false;
  args.rval().setObject(*promise);
  return true;
}
}  // namespace ruyijs::web_audio
