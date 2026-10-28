#include "browser_environment/window_message_options.h"
#include "browser_environment/schema.h"
#include "runtime/runtime.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/ValueArray.h"

namespace ruyijs::browser_environment {
namespace {
bool Sequence(JSContext* cx, JS::HandleValue input, JS::MutableHandleValue output) {
  output.setUndefined();
  if (input.isUndefined()) return true;
  if (!input.isObject()) return Error(cx, "postMessage transfer must be a sequence of objects");
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(input)) return false;
  JS::RootedValueVector values(cx);
  JS::RootedValue value(cx);
  bool done = false;
  while (true) {
    if (!iterator.next(&value, &done)) return false;
    if (done) break;
    if (!value.isObject()) return Error(cx, "postMessage transfer entries must be objects");
    if (!values.append(value)) return false;
  }
  JS::RootedObject array(cx, JS::NewArrayObject(cx, values));
  if (!array) return false;
  output.setObject(*array);
  return true;
}
}
bool MessageOptions(JSContext* cx, const JS::CallArgs& args, std::string* origin,
                    JS::MutableHandleValue transfer) {
  if (!args.length()) return Error(cx, "postMessage requires a message");
  JS::RootedValue option(cx, args.get(1)), value(cx);
  JS::RootedString text(cx);
  if (args.length() < 3 && (option.isObject() || option.isNullOrUndefined())) {
    // StructuredSerializeOptions is the base dictionary: transfer precedes targetOrigin.
    JS::RootedObject dict(cx, option.isObject() ? &option.toObject() : nullptr);
    if (dict && !JS_GetProperty(cx, dict, "transfer", &value)) return false;
    if (!Sequence(cx, value, transfer)) return false;
    value.setUndefined();
    if (dict && !JS_GetProperty(cx, dict, "targetOrigin", &value)) return false;
    if (value.isUndefined()) { *origin = "/"; return true; }
    text = JS::ToString(cx, value);
  } else {
    text = JS::ToString(cx, option);
    if (!text || !Sequence(cx, args.get(2), transfer)) return false;
  }
  return text && ToUTF8(cx, text, origin);
}
}
