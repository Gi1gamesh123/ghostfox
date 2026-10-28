#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::document_model {
bool ReadShadowOptions(JSContext* cx, JS::HandleValue value, ShadowOptions* result) {
  if (!value.isObject()) return TypeError(cx, "ShadowRootInit requires a mode");
  JS::RootedObject options(cx, &value.toObject());
  JS::RootedValue item(cx);
  ShadowOptions converted;
  if (!JS_GetProperty(cx, options, "clonable", &item)) return false;
  converted.clonable = JS::ToBoolean(item);
  if (!JS_GetProperty(cx, options, "delegatesFocus", &item)) return false;
  converted.delegates_focus = JS::ToBoolean(item);
  if (!JS_GetProperty(cx, options, "mode", &item)) return false;
  if (item.isUndefined()) return TypeError(cx, "ShadowRootInit mode is required");
  DomString mode;
  if (!String(cx, item, &mode)) return false;
  if (mode != u"open" && mode != u"closed") return TypeError(cx, "Invalid shadow root mode");
  converted.closed = mode == u"closed";
  if (!JS_GetProperty(cx, options, "serializable", &item)) return false;
  converted.serializable = JS::ToBoolean(item);
  if (!JS_GetProperty(cx, options, "slotAssignment", &item)) return false;
  if (!item.isUndefined()) {
    DomString assignment;
    if (!String(cx, item, &assignment)) return false;
    if (assignment != u"named" && assignment != u"manual") return TypeError(cx, "Invalid slot assignment mode");
    converted.manual_slots = assignment == u"manual";
  }
  *result = converted;
  return true;
}
}
