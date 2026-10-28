#include "web_workers/child.h"
#include "web_workers/clone.h"
#include "web_workers/error_event.h"
#include "runtime/pipe.h"
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"
#include <cstring>

namespace ruyijs::web_workers {
namespace {
JSString* Text(JSContext* cx, const char* text) {
  if (!text) text = "";
  size_t length = 0;
  while (length < 4096 && text[length]) ++length;
  while (length && (static_cast<unsigned char>(text[length]) & 0xc0) == 0x80) --length;
  return JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text, length));
}
}
bool Child::ReportError(JS::HandleValue error, JS::HandleObject stack) {
  JS::ExceptionStack exception(cx_, error, stack);
  JS::ErrorReportBuilder report(cx_);
  if (!report.init(cx_, exception, JS::ErrorReportBuilder::NoSideEffects)) return false;
  JS::RootedObject init(cx_, JS_NewObjectWithGivenProto(cx_, nullptr, nullptr));
  JS::RootedString message(cx_, Text(cx_, report.toStringResult().c_str()));
  JS::RootedString filename(cx_, Text(cx_, report.report()->filename.c_str()));
  if (!init || !message || !filename ||
      !JS_DefineProperty(cx_, init, "message", message, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx_, init, "filename", filename, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx_, init, "lineno", double(report.report()->lineno), JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx_, init, "colno", double(report.report()->column.oneOriginValue()), JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx_, init, "bubbles", false, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx_, init, "composed", false, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx_, init, "cancelable", true, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx_, init, "error", error, JSPROP_ENUMERATE)) return false;
  bool allowed = true;
  if (!reportingError_) {
    reportingError_ = true;
    const bool ok = FireError(cx_, kit_.page, init, errorPrototype_, &allowed);
    reportingError_ = false;
    if (!ok) return false;
  }
  if (!allowed) return true;
  // Error identity is local to its Worker realm. The parent receives null.
  if (!JS_DefineProperty(cx_, init, "error", JS::NullHandleValue, JSPROP_ENUMERATE)) return false;
  JS::RootedValue payload(cx_, JS::ObjectValue(*init));
  std::string frame;
  if (!EncodeMessage(cx_, exception_, payload, JS::UndefinedHandleValue, &frame)) return false;
  frame[0] = '\x03';
  return pipe_.Queue(std::move(frame));
}
}
