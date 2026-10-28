// Shared public DOMImplementation conversion, moved from doctype_factory.cpp.
#include "document_model/html_control.h"
#include "js/Conversions.h"
#include "js/Exception.h"
namespace ruyijs::document_model {
bool PublicDocumentString(JSContext* cx, JS::HandleValue value, DomString* text) {
  if (value.isBigInt()) { text->clear(); return true; }
  if (value.isNumber()) return ControlString(cx, value, text);
  JS::RootedString converted(cx, JS::ToString(cx, value));
  if (!converted) {
    if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
    JS_ClearPendingException(cx);
    *text = value.isObject() ? u"[object Object]" : u"";
    return true;
  }
  JS::RootedValue string(cx, JS::StringValue(converted));
  return String(cx, string, text);
}
}
