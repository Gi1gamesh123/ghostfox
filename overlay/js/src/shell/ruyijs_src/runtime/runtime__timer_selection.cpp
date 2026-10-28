#include "runtime/scheduling_control.h"
#include "runtime/environment_control.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include <cmath>
#include <climits>
#include <initializer_list>

namespace ruyijs {
using execution_scheduling::Error;
namespace {
bool Fields(JSContext* cx, JS::HandleObject request, std::initializer_list<const char*> allowed) {
  JS::Rooted<JS::IdVector> keys(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, request, &keys)) return false;
  for (size_t i = 0; i < keys.length(); ++i) {
    JS::RootedId id(cx, keys[i]); JS::RootedValue value(cx);
    if (!JS_IdToValue(cx, id, &value)) return false;
    JS::RootedString key(cx, JS::ToString(cx, value)); std::string name;
    if (!key || !ToUTF8(cx, key, &name)) return false;
    if (name == "id" || name == "op" || name == "action") continue;
    bool found = false; for (const char* item : allowed) if (name == item) found = true;
    if (!found) return Error(cx, "Unknown timer selection field");
  }
  return true;
}
bool Number(JSContext* cx, JS::HandleObject request, const char* name, double* output) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, request, name, &value)) return false;
  if (!value.isNumber() || !std::isfinite(value.toNumber())) return Error(cx, "Timer selection requires finite numbers");
  *output = value.toNumber(); return true;
}
struct List {
  JSContext* cx;
  JS::RootedObject array;
  uint32_t count = 0;
  explicit List(JSContext* context) : cx(context), array(context, JS::NewArrayObject(context, 0)) {}
};
bool Append(void* context, const execution_scheduling::TimerSnapshot& value) {
  auto& list = *static_cast<List*>(context);
  JSContext* cx = list.cx;
  JS::RootedObject row(cx, JS_NewPlainObject(cx));
  JS::RootedValue repeating(cx, JS::BooleanValue(value.repeating)), ready(cx, JS::BooleanValue(value.ready));
  if (!row || !JS_DefineProperty(cx, row, "task_id", double(value.taskId), JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, row, "realm_id", value.realmId, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, row, "public_handle", value.publicHandle, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, row, "firings", value.firings, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, row, "due_ms", value.deadline, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, row, "repeating", repeating, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, row, "ready", ready, JSPROP_ENUMERATE)) return false;
  return JS_SetElement(cx, list.array, list.count++, row);
}
}
bool SchedulingControl::TimerSelection(JS::HandleObject request, const std::string& action,
                                        JS::MutableHandleValue result) {
  if (action == "configure_timer_replay") {
    if (!Fields(cx_, request, {"enabled"})) return false;
    JS::RootedValue enabled(cx_);
    if (!JS_GetProperty(cx_, request, "enabled", &enabled)) return false;
    if (!enabled.isBoolean()) return Error(cx_, "Timer replay enabled must be boolean");
    if (environment_.Frozen()) return Error(cx_, "Timer replay configuration is frozen");
    if (!owner_.ConfigureTimerReplay(enabled.toBoolean())) return false;
    result.set(enabled); return true;
  }
  if (action == "pending_timers") {
    if (!Fields(cx_, request, {})) return false;
    List list(cx_);
    if (!list.array || !owner_.VisitPendingTimers(Append, &list)) return false;
    result.setObject(*list.array); return true;
  }
  if (!Fields(cx_, request, {"task_id", "expected_firings", "at_ms"})) return false;
  double id, firings, at;
  if (!Number(cx_, request, "task_id", &id) || !Number(cx_, request, "expected_firings", &firings) ||
      !Number(cx_, request, "at_ms", &at)) return false;
  if (id < 1 || id > INT32_MAX || std::trunc(id) != id || firings < 0 || firings >= INT32_MAX || std::trunc(firings) != firings)
    return Error(cx_, "Timer selection identity must be a bounded integer", true);
  if (!owner_.ValidateTimerSelection(uint64_t(id), uint32_t(firings), at)) return false;
  environment_.Freeze();
  execution_scheduling::RunResult executed;
  if (!owner_.RunSelectedTimer(uint64_t(id), uint32_t(firings), at, &executed)) return false;
  return Describe(executed, result);
}
}
