#include "runtime/console_control.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <string>

namespace ruyijs {
namespace {
bool DefineText(JSContext* cx, JS::HandleObject object, const char* key,
                const std::string& value) {
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(
      cx, JS::UTF8Chars(value.data(), value.size())));
  return text && JS_DefineProperty(cx, object, key, text, JSPROP_ENUMERATE);
}

bool Entry(JSContext* cx, const web_console::Record& record,
           JS::MutableHandleObject result) {
  JS::RootedObject entry(cx, JS_NewPlainObject(cx));
  JS::RootedObject stack(cx, JS::NewArrayObject(cx, record.stack.size()));
  if (!entry || !stack) return false;
  uint32_t index = 0;
  for (const std::string& frame : record.stack) {
    JS::RootedString text(cx, JS_NewStringCopyUTF8N(
        cx, JS::UTF8Chars(frame.data(), frame.size())));
    if (!text || !JS_DefineElement(cx, stack, index++, text, JSPROP_ENUMERATE))
      return false;
  }
  if (!DefineText(cx, entry, "method", record.method) ||
      !DefineText(cx, entry, "level", record.level) ||
      !DefineText(cx, entry, "text", record.text) ||
      !JS_DefineProperty(cx, entry, "realm", static_cast<double>(record.realm),
                         JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, entry, "indent", static_cast<double>(record.indent),
                         JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, entry, "stack", stack, JSPROP_ENUMERATE)) return false;
  result.set(entry);
  return true;
}

bool Describe(JSContext* cx, const web_console::Log& log,
              JS::MutableHandleValue result) {
  JS::RootedObject array(cx, JS::NewArrayObject(cx, log.records().size()));
  JS::RootedObject object(cx, JS_NewPlainObject(cx));
  if (!array || !object) return false;
  uint32_t index = 0;
  for (const web_console::Record& record : log.records()) {
    JS::RootedObject entry(cx);
    if (!Entry(cx, record, &entry) ||
        !JS_DefineElement(cx, array, index++, entry, JSPROP_ENUMERATE)) return false;
  }
  if (!JS_DefineProperty(cx, object, "entries", array, JSPROP_ENUMERATE)) return false;
  result.setObject(*object);
  return true;
}
}

bool ConsoleControl::Request(JS::HandleObject request, JS::MutableHandleValue result) {
  std::string action;
  if (!StringProperty(cx_, request, "action", &action)) return false;
  if (action == "log") {
    JS::RootedValue drain(cx_);
    if (!JS_GetProperty(cx_, request, "clear", &drain)) return false;
    if (!drain.isUndefined() && !drain.isBoolean()) {
      JS_ReportErrorASCII(cx_, "console clear must be a boolean");
      return false;
    }
    if (!Describe(cx_, owner_, result)) return false;
    if (drain.isBoolean() && drain.toBoolean()) owner_.ClearAll();
    return true;
  }
  if (action == "clear") {
    owner_.ClearAll();
    return Describe(cx_, owner_, result);
  }
  JS_ReportErrorASCII(cx_, "Unknown console action");
  return false;
}
}
