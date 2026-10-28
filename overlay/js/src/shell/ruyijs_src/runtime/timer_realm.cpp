#include "runtime/scheduling_control.h"
#include "runtime/environment_control.h"
#include "runtime/resource_schema.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/Realm.h"
#include "js/StableStringChars.h"
#include "js/PropertyAndElement.h"

namespace ruyijs {
bool SchedulingControl::TimerRealm(JS::HandleObject page, JS::HandleObject request,
                                   JS::MutableHandleValue result) {
  if (!resource_wire::Fields(cx_, request, {"id", "op", "target"})) return false;
  JS::RootedValue expression(cx_);
  if (!JS_GetProperty(cx_, request, "target", &expression)) return false;
  if (!expression.isString()) return execution_scheduling::Error(cx_, "Timer realm target must be an expression string");
  JS::RootedString text(cx_, expression.toString());
  JS::AutoStableStringChars chars(cx_);
  if (!chars.initTwoByte(cx_, text)) return false;
  if (!chars.length() || chars.length() > 65536)
    return execution_scheduling::Error(cx_, "Timer realm expression must be 1..65536 UTF-16 units", true);
  if (!owner_.ValidateTimerRealmQuery()) return false;
  environment_.Freeze(); owner_.Freeze();
  JSAutoRealm realm(cx_, page);
  JS::RootedValue value(cx_);
  if (!EvaluateHostExpression(cx_, "<timer-realm>", chars.twoByteChars(), chars.length(), &value)) return false;
  if (!value.isObject()) return execution_scheduling::Error(cx_, "Timer realm target is not an object");
  JS::RootedObject object(cx_, &value.toObject());
  uint32_t identity;
  if (!owner_.TimerRealmIdentity(object, &identity)) return false;
  result.setNumber(identity);
  return true;
}
}
