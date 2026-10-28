// Shared C/Python host entry. The Interaction owner creates and dispatches input.
#include "runtime/input_control.h"
#include "runtime/runtime.h"
#include "runtime/scheduling_control.h"
#include "runtime/resource_schema.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/StableStringChars.h"

namespace ruyijs {
bool InputRequest(JSContext* cx, JS::HandleObject page, JS::HandleObject request,
                  SchedulingControl& scheduling, JS::MutableHandleValue result) {
  std::string type;
  JS::RootedValue expression(cx), options(cx);
  if (!resource_wire::Fields(cx, request, {"id", "op", "type", "target", "init"}) ||
      !resource_wire::Property(cx, request, "type", &type, 64, resource_wire::TextKind::ASCII) ||
      !JS_GetProperty(cx, request, "target", &expression) ||
      !JS_GetProperty(cx, request, "init", &options)) return false;
  if (!expression.isString() || !options.isObject())
    return resource_wire::Error(cx, "Input needs a target expression and an init dictionary");
  JS::RootedString text(cx, expression.toString());
  JS::AutoStableStringChars characters(cx);
  if (!characters.initTwoByte(cx, text)) return false;
  if (characters.length() > 65536)
    return resource_wire::Error(cx, "Input target expression exceeds 64 KiB", true);
  {
    JSAutoRealm realm(cx, page);
    JS::RootedValue target(cx);
    if (!EvaluateHostExpression(cx, "<input-target>", characters.twoByteChars(), characters.length(), &target))
      return false;
    if (!target.isObject()) return resource_wire::Error(cx, "Input target is not an object");
    JS::RootedObject object(cx, &target.toObject()), init(cx, &options.toObject());
    if (!interaction::DispatchInput(cx, object, type.c_str(), init,
                                   scheduling.ClockMilliseconds())) return false;
  }
  result.setBoolean(true);
  return true;
}
}
