#include "runtime/runtime.h"
#include "runtime/exception_capture.h"
#include "runtime_assets.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/CompilationAndEvaluation.h"
#include "js/JSON.h"
#include "js/PropertyAndElement.h"
#include "js/SourceText.h"
#include "js/String.h"
#include "js/ValueArray.h"

namespace ruyijs {
bool InitCodec(JSContext* cx, JS::MutableHandleObject codec) {
  JS::CompileOptions options(cx);
  options.setFileAndLine("ruyijs:codec", 1);
  JS::SourceText<mozilla::Utf8Unit> source;
  if (!source.init(cx, kCodecSource, sizeof(kCodecSource) - 1,
                   JS::SourceOwnership::Borrowed)) return false;
  JS::RootedValue result(cx);
  if (!JS::Evaluate(cx, options, source, &result) || !result.isObject())
    return false;
  codec.set(&result.toObject());
  return true;
}

bool ParseRequest(JSContext* cx, const std::string& input,
                  JS::MutableHandleObject request) {
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx,
      JS::UTF8Chars(input.data(), input.size())));
  JS::RootedValue value(cx);
  if (!text || !JS_ParseJSON(cx, text, &value)) return false;
  if (!value.isObject()) {
    JS_ReportErrorASCII(cx, "RPC request must be an object");
    return false;
  }
  request.set(&value.toObject());
  return true;
}

bool StringProperty(JSContext* cx, JS::HandleObject object, const char* key,
                    std::string* output, const char* fallback) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, object, key, &value)) return false;
  if (value.isUndefined() && fallback) {
    *output = fallback;
    return true;
  }
  if (!value.isString()) {
    JS_ReportErrorASCII(cx, "RPC field %s must be a string", key);
    return false;
  }
  JS::RootedString text(cx, value.toString());
  return ToUTF8(cx, text, output);
}

bool EncodeReply(JSContext* cx, JS::HandleObject codec, JS::HandleValue id,
                 bool success, JS::HandleValue value, std::string* output, bool* delivered) {
  JS::RootedValueArray<3> arguments(cx);
  arguments[0].set(id);
  arguments[1].setBoolean(success);
  arguments[2].set(value);
  JS::RootedValue result(cx);
  if (!JS_CallFunctionName(cx, codec, "reply", arguments, &result)) {
    if (!FormatOperationFailure(cx, arguments[2])) return false;
    arguments[1].setBoolean(false);
    success = false;
    if (!JS_CallFunctionName(cx, codec, "reply", arguments, &result)) return false;
  }
  if (!result.isString()) return false;
  JS::RootedString encoded(cx, result.toString());
  if (delivered) *delivered = success;
  return ToUTF8(cx, encoded, output);
}
}
