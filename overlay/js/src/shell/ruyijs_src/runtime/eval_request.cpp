#include "runtime/runtime_internal.h"
#include "runtime/host_values.h"
#include <cmath>
#include <climits>
#include "jsapi.h"
#include "js/PropertyAndElement.h"

namespace ruyijs {
static bool Offset(JSContext* cx, JS::HandleObject request, const char* name, int* result) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, request, name, &value)) return false;
  if (value.isUndefined()) { *result = -1; return true; }
  if (!value.isNumber() || !std::isfinite(value.toNumber()) ||
      std::trunc(value.toNumber()) != value.toNumber() ||
      value.toNumber() < INT_MIN || value.toNumber() > INT_MAX)
    return HostValueError(cx, "Evaluation offset must be a signed 32-bit integer");
  *result = int(value.toNumber());
  return true;
}
bool EvalRequest(JSContext* cx, JS::HandleObject page, JS::HandleObject request,
                 HostValues& values, Checkpoint checkpoint,
                 JS::MutableHandleValue result, bool* success) {
  JS::RootedValue source(cx), toPython(cx);
  std::string filename, mode;
  int line = -1, col = -1;
  *success = JS_GetProperty(cx, request, "code", &source) &&
      StringProperty(cx, request, "filename", &filename, "<eval>") &&
      StringProperty(cx, request, "result_mode", &mode, "typed") &&
      JS_GetProperty(cx, request, "to_py", &toPython) &&
      Offset(cx, request, "line", &line) && Offset(cx, request, "col", &col);
  if (*success && mode != "typed" && mode != "public")
    *success = HostValueError(cx, "Unknown evaluation result mode");
  if (*success && !toPython.isUndefined() && !toPython.isBoolean())
    *success = HostValueError(cx, "Evaluation to_py must be a boolean");
  if (*success && !source.isString())
    *success = HostValueError(cx, "RPC code must be a string");
  if (*success && filename.find('\0') != std::string::npos)
    *success = HostValueError(cx, "RPC filename contains NUL");
  if (!*success) return FormatOperationFailure(cx, result);
  EvalOptions options(filename, line, col, toPython.isTrue(), false);
  JS::RootedString code(cx, source.toString());
  if (!Evaluate(cx, page, code, filename.c_str(), checkpoint, result, success, options, nullptr, true)) return false;
  if (*success && mode == "public") {
    JS::RootedValue encoded(cx);
    *success = values.Encode(result, options.to_py(), &encoded);
    if (!*success) return FormatOperationFailure(cx, result);
    result.set(encoded);
  }
  return true;
}
}
