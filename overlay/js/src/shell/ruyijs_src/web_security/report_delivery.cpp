// CSP report tasks on the existing Scheduling owner; no independent queue.
#include "web_security/state.h"
#include "document_model/realm.h"
#include "interaction/realm.h"
#include "js/Realm.h"
#include "jsfriendapi.h"

namespace ruyijs::web_security {
namespace {
const JSClass kReport = {"CSP report", JSCLASS_HAS_RESERVED_SLOTS(3)};
bool DeliverReport(State* state, JS::HandleObject report) {
  JSContext* cx = state->cx;
  if (JS::GetReservedSlot(report, 2).toNumber() != double(state->generation)) return true;
  JS::RootedObject target(cx, JS::GetReservedSlot(report, 0).toObjectOrNull());
  JS::RootedObject init(cx, &JS::GetReservedSlot(report, 1).toObject());
  // The element can be removed or adopted between the execution check and
  // this task. Firefox resolves the target at delivery, not at the check.
  if (!target || !state->document.NodeConnected(target)) target = state->document.document();
  return !target || state->events.FirePolicyViolation(target, init);
}
bool Deliver(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  JS::RootedObject report(cx, &js::GetFunctionNativeReserved(&args.callee(), 1).toObject());
  args.rval().setUndefined();
  return DeliverReport(state, report);
}
}
bool QueueReport(State& state, JS::HandleObject element, JS::HandleObject init) {
  if (!state.task) return true;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject report(cx, JS_NewObject(cx, &kReport));
  if (!report) return false;
  JS::SetReservedSlot(report, 0, JS::ObjectOrNullValue(element));
  JS::SetReservedSlot(report, 1, JS::ObjectValue(*init));
  JS::SetReservedSlot(report, 2, JS::NumberValue(double(state.generation)));
  if (state.holding) {
    if (!state.held.append(report)) { JS_ReportOutOfMemory(cx); return false; }
    return true;
  }
  JS::RootedFunction fn(cx, js::NewFunctionWithReserved(cx, Deliver, 0, 0, "CSP report delivery"));
  if (!fn) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(&state));
  js::SetFunctionNativeReserved(task, 1, JS::ObjectValue(*report));
  return state.task(state.taskContext, task);
}
bool FlushHeldReports(State& state) {
  state.holding = false;
  JSAutoRealm realm(state.cx, state.page);
  JS::RootedObject report(state.cx);
  for (size_t i = 0; i < state.held.length(); ++i) {
    report = state.held[i];
    if (!DeliverReport(&state, report)) { state.held.clear(); return false; }
  }
  state.held.clear();
  return true;
}
}
