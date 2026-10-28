#include "execution_scheduling/state.h"
#include "execution_scheduling/idle.h"
#include "execution_scheduling/scheduler.h"
#include "runtime/exception_capture.h"
#include "js/CallAndConstruct.h"
#include "js/CompilationAndEvaluation.h"
#include "js/Exception.h"
#include "js/Realm.h"
#include "js/SourceText.h"
#include "js/StableStringChars.h"

namespace ruyijs::execution_scheduling {
void State::CheckpointJobs() {
  checkpoint(cx);
  if (JS_IsExceptionPending(cx)) {
    CaptureOperationFailure(cx);
    JS_ClearPendingException(cx);
  }
}
static bool EvaluateHandler(JSContext* cx, JS::HandleString code, JS::MutableHandleValue result) {
  JS::AutoStableStringChars characters(cx);
  if (!characters.initTwoByte(cx, code)) return false;
  JS::CompileOptions options(cx);
  options.setFileAndLine("ruyijs:timer", 1);
  JS::SourceText<char16_t> source;
  return source.init(cx, characters.twoByteChars(), characters.length(), JS::SourceOwnership::Borrowed) &&
         JS::Evaluate(cx, options, source, result);
}
void State::Invoke(const TaskSnapshot& task) {
  Callback* callback = Find(task.id);
  if (!callback) { model.Complete(task.id); return; }
  if (callback->global && Retired(callback->global)) {
    model.Complete(task.id);
    if (model.Exists(task.id)) Drop(task.id, task.kind);
    else Remove(task.id);
    return;
  }
  callback->running = true;
  if (timerReplay && callback->publicHandle &&
      (callback->kind == Kind::Timeout || callback->kind == Kind::Interval)) ++callback->replayFirings;
  uint64_t previousWebTask=runningWebTask;
  runningWebTask=task.kind==Kind::WebTask?task.id:0;
  {
    // A callback runs in the realm it belongs to: a string handler compiles
    // there and a timer's `this` is that Window, as in Firefox. A keepalive
    // task has no document and runs in the scheduling owner's own realm.
    JS::RootedObject home(cx, callback->global ? callback->global : page.get());
    JSAutoRealm realm(cx, home);
    JS::RootedValue handler(cx, callback->handler), result(cx);
    bool ok;
    if (handler.isString()) {
      JS::RootedString code(cx, handler.toString());
      ok = EvaluateHandler(cx, code, &result);
    } else if (task.kind == Kind::WebTask) {
      ok = InvokeWebTask(*this,*callback,&result);
    } else if (task.kind == Kind::Idle) {
      ok = InvokeIdle(*this,*callback,task,&result);
    } else if (task.kind == Kind::Frame) {
      JS::RootedValue stamp(cx, JS::DoubleValue(task.timestamp));
      ok = JS::Call(cx, JS::UndefinedHandleValue, handler, JS::HandleValueArray(stamp), &result);
    } else {
      JS::RootedValue receiver(cx, JS::ObjectValue(*home));
      auto arguments = JS::HandleValueArray::fromMarkedLocation(
          callback->arguments.length(), callback->arguments.begin());
      ok = JS::Call(cx, receiver, handler, arguments, &result);
    }
    if (!ok) { CaptureOperationFailure(cx); JS_ClearPendingException(cx); }
    CheckpointJobs();
  }
  callback->running = false;
  runningWebTask=previousWebTask;
  SyncClock();
  model.Complete(task.id);
  if (!model.Exists(task.id)) Remove(task.id);
}
}
