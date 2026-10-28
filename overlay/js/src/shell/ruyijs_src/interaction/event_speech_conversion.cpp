#include "interaction/internal.h"
#include "runtime/page_helpers.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include <cmath>
#include <cstdio>

namespace ruyijs::interaction {
bool PublicEventString(JSContext* cx, JS::HandleValue value, const char* fallback,
                   JS::MutableHandleString out) {
  if (value.isSymbol() || value.isBigInt() || (fallback && value.isUndefined())) {
    out.set(JS_NewStringCopyZ(cx, fallback && !value.isSymbol() ? fallback : ""));
  } else if (value.isNumber() && !(std::isfinite(value.toNumber()) &&
             std::trunc(value.toNumber()) == value.toNumber() &&
             value.toNumber() >= INT32_MIN && value.toNumber() <= INT32_MAX)) {
    const double number = value.toNumber();
    char buffer[400];
    if (std::isnan(number)) std::snprintf(buffer, sizeof(buffer), "nan");
    else if (std::isinf(number)) std::snprintf(buffer, sizeof(buffer), number < 0 ? "-inf" : "inf");
    else std::snprintf(buffer, sizeof(buffer), "%.6f", number);
    out.set(JS_NewStringCopyZ(cx, buffer));
  } else {
    out.set(JS::ToString(cx, value));
    if (!out) {
      if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
      JS_ClearPendingException(cx);
      out.set(JS_NewStringCopyZ(cx, fallback ? fallback : "[object Object]"));
    }
  }
  if (!out) return false;
  if (JS_GetStringLength(out) > 1024 * 1024)
    return Error(cx, "RangeError", "Speech event text exceeds limit");
  return true;
}
bool PublicFormattedEventType(JSContext* cx, JS::HandleValue value,
                      JS::MutableHandleString out, std::string* key) {
  if (!PublicEventString(cx, value, nullptr, out)) return false;
  JS::RootedValue text(cx, JS::StringValue(out));
  return Text(cx, text, out, key);
}
namespace {
bool Trusted(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx, args.thisv().isObject() ? &args.thisv().toObject() : nullptr);
  if (object) object = js::CheckedUnwrapStatic(object);
  State* owner = object ? Owner(cx, object, true) : nullptr;
  Event* event = owner ? owner->event(object) : nullptr;
  args.rval().setUndefined();
  if (event && JS::GetReservedSlot(event->object, kEventPublicSecond).isObject()) {
    args.rval().setBoolean(event->value.is_trusted());
    return true;
  }
  if (event && event->kind == Kind::SpeechError) {
    ReportEventAccess(cx, event, "SpeechSynthesisErrorEvent", "isTrusted", int(ApiAccess::Read));
    args.rval().setBoolean(false);
  }
  return true;
}
}
bool InstallSpeechTrusted(State& state, JS::HandleObject object, Kind kind) {
  const bool error = kind == Kind::SpeechError;
  auto& cached = error ? state.speechErrorTrustedGetter : state.speechTrustedGetter;
  if (!cached) {
    JSFunction* getter = JS_NewFunction(state.cx, Trusted, 0, 0, "get isTrusted");
    if (!getter) return false;
    cached = JS_GetFunctionObject(getter);
    if (!state.helpers->Member(cached, error ? "SpeechSynthesisErrorEvent" : "SpeechSynthesisEvent",
                                "get isTrusted", "get")) return false;
  }
  JS::RootedObject setter(state.cx);
  return JS_DefineProperty(state.cx, object, "isTrusted", cached, setter,
                           JSPROP_ENUMERATE | (error ? 0 : JSPROP_PERMANENT));
}
}
