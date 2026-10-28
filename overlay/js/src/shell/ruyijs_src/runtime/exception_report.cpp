#include "runtime/exception_capture.h"
#include <tuple>
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"
#include "js/SavedFrameAPI.h"
#include "js/String.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
#include "vm/ErrorObject.h"


namespace ruyijs {
static std::string StringPrefix(JSContext* cx, JSString* string, size_t limit) {
  if (!string) return "";
  std::string output(limit, '\0');
  auto encoded = JS_EncodeStringToUTF8BufferPartial(cx, string,
      mozilla::Span<char>(output.data(), output.size()));
  if (!encoded) return "";
  output.resize(std::get<1>(*encoded));
  return output;
}

static std::string TextPrefix(const char* text, size_t limit) {
  if (!text) return "";
  size_t length = 0;
  while (length < limit && text[length]) ++length;
  while (length && (static_cast<unsigned char>(text[length]) & 0xc0) == 0x80)
    --length;
  return std::string(text, length);
}

static void StringMessage(JSContext* cx, JS::HandleString string,
                            ExceptionReport* report, size_t limit = 4096) {
  std::string bytes(limit, '\0');
  auto encoded = JS_EncodeStringToUTF8BufferPartial(cx, string,
      mozilla::Span<char>(bytes.data(), bytes.size()));
  if (!encoded) return;
  bytes.resize(std::get<1>(*encoded));
  report->message = std::move(bytes);
  size_t units = std::get<0>(*encoded);
  report->messageUnits.resize(units);
  for (size_t index = 0; index < units; ++index) {
    if (!JS_GetStringCharAt(cx, string, index, &report->messageUnits[index])) {
      report->messageUnits.clear();
      return;
    }
  }
  report->hasMessageUnits = true;
}

static const char* ErrorName(int type) {
  switch (type) {
    case JSEXN_INTERNALERR: return "InternalError";
    case JSEXN_AGGREGATEERR: return "AggregateError";
    case JSEXN_EVALERR: return "EvalError";
    case JSEXN_RANGEERR: return "RangeError";
    case JSEXN_REFERENCEERR: return "ReferenceError";
    case JSEXN_SUPPRESSEDERR: return "SuppressedError";
    case JSEXN_SYNTAXERR: return "SyntaxError";
    case JSEXN_TYPEERR: return "TypeError";
    case JSEXN_URIERR: return "URIError";
    case JSEXN_WASMCOMPILEERROR: return "CompileError";
    case JSEXN_WASMLINKERROR: return "LinkError";
    case JSEXN_WASMRUNTIMEERROR: return "RuntimeError";
    default: return "Error";
  }
}

static std::string SavedStack(JSContext* cx, JS::HandleObject stack) {
  std::string output;
  JS::RootedObject frame(cx, stack), parent(cx);
  JS::RootedString name(cx), source(cx);
  for (size_t count = 0; frame && count < 32; ++count) {
    uint32_t line = 0;
    if (JS::GetSavedFrameSource(cx, nullptr, frame, &source) !=
        JS::SavedFrameResult::Ok) break;
    JS::GetSavedFrameFunctionDisplayName(cx, nullptr, frame, &name);
    JS::GetSavedFrameLine(cx, nullptr, frame, &line);
    std::string entry = StringPrefix(cx, name, 128) + "@" +
        StringPrefix(cx, source, 1024) + ":" + std::to_string(line) + "\n";
    if (output.size() + entry.size() > 16384) break;
    output += entry;
    if (JS::GetSavedFrameParent(cx, nullptr, frame, &parent) !=
        JS::SavedFrameResult::Ok) break;
    frame = parent;
  }
  return output;
}

ExceptionReport DescribeException(JSContext* cx, JS::HandleValue value,
                                   JS::HandleObject stack) {
  ExceptionReport result;
  JS::AutoSaveExceptionState saved(cx);
  JS::RootedObject originalStack(cx, stack);
  if (value.isObject()) {
    JS::RootedObject object(cx, &value.toObject());
    if (JSObject* errorStack = JS::ExceptionStackOrNull(object))
      originalStack = errorStack;
    JS::ExceptionStack exception(cx, value, originalStack);
    JS::ErrorReportBuilder report(cx);
    if (report.init(cx, exception, JS::ErrorReportBuilder::NoSideEffects)) {
      result.name = ErrorName(report.report()->exnType);
      result.message = TextPrefix(report.report()->message().c_str(), 4096);
    }
    JS::RootedObject unwrapped(cx, js::CheckedUnwrapStatic(object));
    if (unwrapped && unwrapped->is<js::ErrorObject>()) {
      JS::RootedString message(cx, unwrapped->as<js::ErrorObject>().getMessage());
      if (message) StringMessage(cx, message, &result);
    }
    JS::RootedString nativeName(cx), nativeMessage(cx);
    if (unwrapped && ReadNativeException(cx, unwrapped, &nativeName, &nativeMessage)
        && nativeName && nativeMessage) {
      ExceptionReport name;
      StringMessage(cx, nativeName, &name, 256);
      result.name = std::move(name.message);
      result.nameUnits = std::move(name.messageUnits);
      result.hasNameUnits = name.hasMessageUnits;
      StringMessage(cx, nativeMessage, &result);
    }
  } else if (value.isSymbol()) {
    JS::Rooted<JS::Symbol*> symbol(cx, value.toSymbol());
    JS::RootedString description(cx, JS::GetSymbolDescription(symbol));
    if (description) StringMessage(cx, description, &result, 4088);
    else { result.message = ""; result.hasMessageUnits = true; }
    result.message = "Symbol(" + result.message + ")";
    result.messageUnits = u"Symbol(" + result.messageUnits + u")";
  } else {
    JS::RootedString string(cx, JS::ToString(cx, value));
    if (string) StringMessage(cx, string, &result);
  }
  JS_ClearPendingException(cx);
  result.stack = SavedStack(cx, originalStack);
  JS_ClearPendingException(cx);
  saved.restore();
  return result;
}

bool CreateExceptionValue(JSContext* cx, const ExceptionReport& report,
                          JS::MutableHandleValue value) {
  JS::RootedObject object(cx, JS_NewPlainObject(cx));
  if (!object) return false;
  const char* keys[] = {"name", "message", "stack"};
  const std::string* fields[] = {&report.name, &report.message, &report.stack};
  for (size_t index = 0; index < 3; ++index) {
    const std::u16string* units = index == 1 && report.hasMessageUnits ? &report.messageUnits :
        index == 0 && report.hasNameUnits ? &report.nameUnits : nullptr;
    JS::RootedString text(cx, units
        ? JS_NewUCStringCopyN(cx, units->data(), units->size())
        : JS_NewStringCopyUTF8N(cx,
            JS::UTF8Chars(fields[index]->data(), fields[index]->size())));
    if (!text || !JS_DefineProperty(cx, object, keys[index], text, JSPROP_ENUMERATE))
      return false;
  }
  value.setObject(*object);
  return true;
}
}
