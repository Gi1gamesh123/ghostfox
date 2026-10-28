// Host user input (ARCHITECTURE.md, host user input). The sequences follow
// Firefox 158 as recorded on a real Google session: a pointer entering from
// outside, down/up/click with click as a PointerEvent (pointerId 0), and per
// character typing (user_keyboard.cpp).
#include "runtime/user_input.h"
#include "runtime/runtime.h"
#include "runtime/user_input_internal.h"
#include "runtime/browsing_context.h"
#include "runtime/resource_schema.h"
#include "runtime/scheduling_control.h"
#include "runtime/top_level.h"
#include "document_model/realm.h"
#include "interaction/realm.h"
#include "web_geometry/realm.h"
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "js/PropertyAndElement.h"
#include "js/StableStringChars.h"
#include <array>
#include <cmath>
#include <string>

namespace ruyijs {
namespace {
using user_input::Init;
// The active window's mozInnerScreenX/Y for the current host action: a real
// pointer's screenX/Y is its client position plus this origin (Firefox trace
// 20261021-160155: screenX - clientX = mozInnerScreenX = 30).
struct { double x = 0, y = 0; } gScreen;
bool Pointer(JSContext* cx, JS::HandleObject target, const char* type, double x, double y,
             int button, int buttons, bool bubbles, SchedulingControl& clock, bool* allowed = nullptr) {
  Init init(cx);
  init.Number("clientX", x).Number("clientY", y).Number("screenX", x + gScreen.x).Number("screenY", y + gScreen.y)
      .Number("button", button).Number("buttons", buttons).Flag("bubbles", bubbles)
      .Flag("cancelable", bubbles).Flag("composed", true).Number("pointerId", 0)
      .Text("pointerType", u"mouse").Flag("isPrimary", true).Number("width", 1).Number("height", 1)
      .Number("pressure", buttons ? 0.5 : 0).Number("detail", std::string(type) == "click" ? 1 : 0);
  return init.Send(target, type, clock, allowed);
}
bool Mouse(JSContext* cx, JS::HandleObject target, const char* type, double x, double y,
           int buttons, int detail, bool bubbles, SchedulingControl& clock, bool* allowed = nullptr) {
  Init init(cx);
  init.Number("clientX", x).Number("clientY", y).Number("screenX", x + gScreen.x).Number("screenY", y + gScreen.y)
      .Number("button", 0).Number("buttons", buttons).Number("detail", detail)
      .Flag("bubbles", bubbles).Flag("cancelable", bubbles).Flag("composed", true);
  return init.Send(target, type, clock, allowed);
}
bool Click(JSContext* cx, BrowsingContext& active, JS::HandleObject target, double hold,
           SchedulingControl& clock) {
  if (!active.environment->InnerScreenOrigin(&gScreen.x, &gScreen.y)) return false;
  std::array<double, 4> rect{};
  bool present = false;
  if (!active.geometry->MeasureElement(target, &rect, &present, false)) JS_ClearPendingException(cx);
  const double x = present ? std::round(rect[0] + rect[2] / 2) : 0;
  const double y = present ? std::round(rect[1] + rect[3] / 2) : 0;
  JS::RootedObjectVector path(cx);
  if (!document_model::ElementPath(cx, target, &path)) return false;
  JS::RootedObject node(cx);
  if (!Pointer(cx, target, "pointerover", x, y, -1, 0, true, clock)) return false;
  for (size_t i = 0; i < path.length(); ++i) {
    node = path[i];
    if (!Pointer(cx, node, "pointerenter", x, y, -1, 0, false, clock)) return false;
  }
  if (!Mouse(cx, target, "mouseover", x, y, 0, 0, true, clock)) return false;
  for (size_t i = 0; i < path.length(); ++i) {
    node = path[i];
    if (!Mouse(cx, node, "mouseenter", x, y, 0, 0, false, clock)) return false;
  }
  bool pointerAllowed = true, mouseAllowed = true, clickAllowed = true;
  if (!Pointer(cx, target, "pointermove", x, y, -1, 0, true, clock) ||
      !Mouse(cx, target, "mousemove", x, y, 0, 0, true, clock) ||
      !Pointer(cx, target, "pointerdown", x, y, 0, 1, true, clock, &pointerAllowed)) return false;
  // A cancelled pointerdown suppresses the compatibility mouse events.
  if (pointerAllowed && !Mouse(cx, target, "mousedown", x, y, 1, 1, true, clock, &mouseAllowed)) return false;
  if (pointerAllowed && mouseAllowed && !document_model::PointerFocus(cx, target)) return false;
  clock.Pass(hold);
  if (!Pointer(cx, target, "pointerup", x, y, 0, 0, true, clock) ||
      (pointerAllowed && !Mouse(cx, target, "mouseup", x, y, 0, 1, true, clock)) ||
      !Pointer(cx, target, "click", x, y, 0, 0, true, clock, &clickAllowed)) return false;
  return !clickAllowed || document_model::Activate(cx, target);
}
bool Delay(JSContext* cx, JS::HandleObject request, const char* name, double* value) {
  bool present = false;
  JS::RootedValue v(cx);
  if (!JS_HasProperty(cx, request, name, &present)) return false;
  if (!present) return true;
  if (!JS_GetProperty(cx, request, name, &v) || !v.isNumber() || !(v.toNumber() >= 0) || v.toNumber() > 10000)
    return resource_wire::Error(cx, "Input delay must be a number from 0 to 10000 ms", true);
  *value = v.toNumber();
  return true;
}
}  // namespace

bool UserInputRequest(JSContext* cx, TopLevel& top, JS::HandleObject request,
                      SchedulingControl& scheduling, JS::MutableHandleValue result) {
  std::string action, text, key, expression;
  double hold = 80, interval = 60;
  bool hasTarget = false;
  if (!resource_wire::Fields(cx, request, {"id", "op", "action", "target", "text", "key", "hold_ms", "interval_ms"}) ||
      !resource_wire::Property(cx, request, "action", &action, 16, resource_wire::TextKind::ASCII) ||
      !JS_HasProperty(cx, request, "target", &hasTarget) ||
      (hasTarget && !resource_wire::Property(cx, request, "target", &expression, 65536)) ||
      !Delay(cx, request, "hold_ms", &hold) || !Delay(cx, request, "interval_ms", &interval)) return false;
  BrowsingContext& active = top.Active();
  JSAutoRealm realm(cx, active.page);
  JS::RootedObject target(cx);
  if (hasTarget) {
    JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(expression.data(), expression.size())));
    JS::AutoStableStringChars characters(cx);
    JS::RootedValue value(cx);
    if (!text || !characters.initTwoByte(cx, text) ||
        !EvaluateHostExpression(cx, "<user-target>", characters.twoByteChars(), characters.length(), &value))
      return false;
    if (!value.isObject()) return resource_wire::Error(cx, "User input target is not an element");
    target = &value.toObject();
  }
  scheduling.Freeze();
  if (action == "click") {
    if (!target) return resource_wire::Error(cx, "click needs a target");
    if (!Click(cx, active, target, hold, scheduling)) return false;
  } else if (action == "type" || action == "press") {
    if (target && !document_model::FocusElement(cx, target)) return false;
    JS::RootedObject focused(cx);
    if (!active.document->ActiveElement(&focused)) return false;
    if (!focused) return resource_wire::Error(cx, "No element has focus");
    if (action == "press") {
      if (!resource_wire::Property(cx, request, "key", &key, 32, resource_wire::TextKind::ASCII) ||
          !user_input::Press(cx, focused, key, scheduling)) return false;
    } else {
      JS::RootedValue raw(cx);
      if (!JS_GetProperty(cx, request, "text", &raw) || !raw.isString())
        return resource_wire::Error(cx, "type needs text");
      JS::RootedString string(cx, raw.toString());
      std::u16string chars(JS_GetStringLength(string), u'\0');
      if (chars.size() > 4096) return resource_wire::Error(cx, "type text exceeds 4096 characters", true);
      if (!JS_CopyStringChars(cx, mozilla::Range<char16_t>(chars.data(), chars.size()), string)) return false;
      for (size_t i = 0; i < chars.size(); ++i) {
        if (i) scheduling.Pass(interval);
        if (!active.document->ActiveElement(&focused) || !focused) break;
        if (!user_input::Character(cx, focused, chars[i], scheduling)) return false;
      }
    }
  } else {
    return resource_wire::Error(cx, "Unknown user input action");
  }
  result.setBoolean(true);
  return true;
}
}  // namespace ruyijs
