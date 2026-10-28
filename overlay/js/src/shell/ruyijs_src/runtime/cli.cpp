#include "runtime/runtime_internal.h"
#include "runtime/arguments.h"
#include <iostream>
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs {
bool Command(JSContext* cx, JS::HandleObject page,
                    JS::HandleObject codec, const char* source,
                    size_t length, const char* filename, Checkpoint checkpoint,
                    ExceptionCapture& errors) {
  errors.BeginRequest();
  JS::RootedString code(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(source, length)));
  JS::RootedValue value(cx), id(cx, JS::NullValue());
  bool success;
  if (!code || !Evaluate(cx, page, code, filename, checkpoint, &value, &success, {}, nullptr, true))
    return false;
  std::string reply;
  if (!Reply(cx, codec, id, success, &value, checkpoint, errors, &reply)) return false;
  std::cout << reply << std::endl;
  JS::RootedObject envelope(cx);
  JS::RootedValue ok(cx);
  return ParseRequest(cx, reply, &envelope) &&
         JS_GetProperty(cx, envelope, "ok", &ok) && ok.isTrue();
}

bool File(JSContext* cx, JS::HandleObject page, JS::HandleObject codec,
                 const char* path, Checkpoint checkpoint, ExceptionCapture& errors) {
  std::string code;
  if (!ReadUTF8File(path, &code)) {
    JS_ReportErrorASCII(cx, "Failed to open ruyijs script");
    return false;
  }
  return Command(cx, page, codec, code.data(), code.size(), path, checkpoint, errors);
}

}
