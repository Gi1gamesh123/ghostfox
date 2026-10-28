// The one error factory: the object comes from the page realm's DOMException, so
// this domain owns no second error type.
#include "web_canvas/internal.h"

namespace ruyijs::web_canvas {
bool ThrowDOMException(State& state, const char* name, const char* message) {
  JSContext* cx = state.cx;
  JS::RootedValue constructor(cx);
  if (!JS_GetProperty(cx, state.kit.page, "DOMException", &constructor) ||
      !constructor.isObject())
    return WebKit::Fail(cx, "canvas requires the page DOMException interface");
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message));
  JS::RootedString label(cx, JS_NewStringCopyZ(cx, name));
  if (!text || !label) return false;
  JS::RootedValueArray<2> arguments(cx);
  arguments[0].setString(text);
  arguments[1].setString(label);
  JS::RootedObject error(cx);
  if (!JS::Construct(cx, constructor, arguments, &error)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*error));
  JS_SetPendingException(cx, value);
  return false;
}
}  // namespace ruyijs::web_canvas
