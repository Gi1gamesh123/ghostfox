#include "web_workers/child.h"
#include "web_workers/clone.h"
#include "web_workers/handlers.h"
#include "web_workers/error_event.h"
#include "interaction/realm.h"
#include "runtime/exception_capture.h"
#include "runtime/page_helpers.h"
#include "runtime/pipe.h"
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <windows.h>

namespace ruyijs::web_workers {
Child::Child(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events, Pipe& pipe, JS::HandleObject exception, WorkerKind kind)
    : cx_(cx), kit_(cx, page, helpers, this), events_(events), pipe_(pipe),
      globalScope_(cx), dedicatedScope_(cx), sharedScope_(cx), name_(cx), exception_(cx, exception),
      handlers_(cx), errorPrototype_(cx), kind_(kind) {}
bool Child::Queue(std::string frame) {
  if (!pipe_.Queue(std::move(frame))) return WebKit::Fail(cx_, "Worker outgoing queue is full or closed");
  const auto status = pipe_.Flush();
  if (status == PipeResult::Failed || status == PipeResult::Closed)
    return WebKit::Fail(cx_, "Worker parent pipe is closed");
  return true;
}
bool Child::Send(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto& child = *static_cast<Child*>(WebKit::Owner(args));
  if (!argc) return WebKit::Fail(cx, "postMessage requires a message");
  if (!args.thisv().isNullOrUndefined() &&
      (!args.thisv().isObject() || &args.thisv().toObject() != child.kit_.page))
    return WebKit::Fail(cx, "Worker global postMessage receiver is invalid");
  std::string frame;
  if (WorkerTokenEnabled()) {
    WorkerMessageToken token{uint64_t(GetCurrentProcessId()), ++child.childSequence_, 1};
    if (!EncodeMessageWithToken(cx, child.exception_, args[0], args.get(1), token, &frame)) return false;
  } else if (!EncodeMessage(cx, child.exception_, args[0], args.get(1), &frame)) return false;
  if (!child.pipe_.Queue(std::move(frame))) return WebKit::Fail(cx, "Worker outgoing queue is full or closed");
  // Submit OS writes now, including when the script next enters a busy loop.
  const auto status = child.pipe_.Flush();
  if (status == PipeResult::Failed || status == PipeResult::Closed)
    return WebKit::Fail(cx, "Worker parent pipe is closed");
  args.rval().setUndefined(); return true;
}
bool Child::Close(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto& child = *static_cast<Child*>(WebKit::Owner(args));
  if (!args.thisv().isNullOrUndefined() &&
      (!args.thisv().isObject() || &args.thisv().toObject() != child.kit_.page))
    return WebKit::Fail(cx, "Worker global close receiver is invalid");
  child.closed_ = true;
  args.rval().setUndefined(); return true;
}
bool Child::Serve(Checkpoint checkpoint, ExceptionCapture& errors, const ChildTasks& tasks,
                   const ChildStartup& startup) {
  JSAutoRealm realm(cx_, kit_.page);
  errors.SetSink([](void* context, JSContext*, JS::HandleValue value, JS::HandleObject stack) {
    return static_cast<Child*>(context)->ReportError(value, stack);
  }, this);
  struct Binding { ExceptionCapture& errors; ~Binding() { errors.SetSink(nullptr, nullptr); } } binding{errors};
  bool booted = false;
  for (;;) {
    if (closed_ && !pipe_.queuedBytes()) return true;
    std::string frame; bool received = false;
    const auto status = NextFrame(&frame, &received);
    if (received && !closed_) {
      errors.BeginRequest();
      if (tasks.begin) tasks.begin(tasks.context);
      JS::RootedValue value(cx_);
      if (!booted) {
        booted = true;
        if (!EvaluateStartup(frame, startup)) return false;
      } else if (!frame.empty() && frame[0] == '\x07') {
        if (broadcastPort_ && !broadcastPort_(broadcastContext_, frame)) return false;
      } else if (shared()) {
        if (!HandleSharedFrame(frame)) return false;
      } else if (service()) {
        if (framePort_ && !framePort_(frameContext_, frame)) return false;
      } else if (WorkerTokenEnabled() && frame.size() && frame[0] == '\x02') {
        if (!DecodeMessageWithToken(cx_, frame, &lastIncoming_, &value) ||
            lastIncoming_.direction != 0 || lastIncoming_.workerId != uint64_t(GetCurrentProcessId()) ||
            !events_.FireMessage(kit_.page, value, "", nullptr)) return false;
      } else if (!DecodeMessage(cx_, frame, &value) || !events_.FireMessage(kit_.page, value, "", nullptr)) {
        return false;
      }
      checkpoint(cx_);
    } else if (status == PipeResult::Closed) {
      return true;
    } else if (status == PipeResult::Failed) {
      return false;
    }
    bool executed = false;
    if (booted && !closed_ && tasks.pump) {
      errors.BeginRequest();
      if (tasks.begin) tasks.begin(tasks.context);
      executed = tasks.pump(tasks.context);
    }
    if (!received && !executed) Sleep(2);
  }
}
}
