#include "document_model/exception_internal.h"
#include "document_model/exception_names.h"
#include "runtime/native_constructor_trace.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <cstring>

namespace ruyijs::document_model {
// Slots: 0 name, 1 message, 2 legacy code, 3 subclass value (undefined for a
// plain DOMException; see CreateDerivedException).
const JSClass kExceptionClass = {"DOMException", JSCLASS_HAS_RESERVED_SLOTS(4)};
bool ExceptionFields(JS::HandleObject object, JS::MutableHandleString name,
                      JS::MutableHandleString message) {
  if (JS::GetClass(object) != &kExceptionClass) return false;
  const auto label = JS::GetReservedSlot(object, 0), text = JS::GetReservedSlot(object, 1);
  if (!label.isString() || !text.isString()) return false;
  name.set(label.toString());message.set(text.toString());return true;
}
// Firefox stores DOMException text as UTF-8. Only unpaired surrogate units are
// replaced at that boundary; ordinary DOMString conversion remains unchanged.
static void Normalize(std::u16string& text) {
  for (size_t index = 0; index < text.size(); ++index) {
    const char16_t unit = text[index];
    if (unit >= 0xd800 && unit <= 0xdbff && index + 1 < text.size() &&
        text[index + 1] >= 0xdc00 && text[index + 1] <= 0xdfff) { ++index;continue; }
    if (unit >= 0xd800 && unit <= 0xdfff) text[index] = 0xfffd;
  }
}
bool CreateException(JSContext* cx, JS::HandleObject installed, std::u16string_view message,
                      std::u16string_view name, JS::MutableHandleValue result, const JS::CallArgs* args) {
  JS::RootedObject prototype(cx, installed);
  if (args) {
    JS::RootedObject target(cx, &args->newTarget().toObject());
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, target, "prototype", &value)) return false;
    if (value.isObject()) prototype = &value.toObject();
  }
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kExceptionClass, prototype));
  JS::RootedValue label(cx), text(cx);
  if (!object || !Value(cx, name, &label) || !Value(cx, message, &text)) return false;
  int code = 0;
  for (size_t index = 0; index < std::size(kExceptionNames); ++index) {
    if (index == 1 || index == 5 || index == 15) continue;
    const char* ascii = kExceptionNames[index].name;
    if (name.size() != strlen(ascii)) continue;
    bool same = true;
    for (size_t unit = 0; unit < name.size(); ++unit) same &= name[unit] == ascii[unit];
    if (same) { code = int(index + 1);break; }
  }
  JS::SetReservedSlot(object, 0, label);JS::SetReservedSlot(object, 1, text);
  JS::SetReservedSlot(object, 2, JS::Int32Value(code));
  result.setObject(*object);return true;
}
bool ThrowException(JSContext* cx, JS::HandleObject prototype, const char* name, const char* message) {
  std::u16string label, text;
  for (; *name; ++name) label.push_back(char16_t(static_cast<unsigned char>(*name)));
  for (; *message; ++message) text.push_back(char16_t(static_cast<unsigned char>(*message)));
  JS::RootedValue value(cx);
  {
    JSAutoRealm realm(cx, prototype);
    if (!CreateException(cx, prototype, text, label, &value)) return false;
  }
  if (!JS_WrapValue(cx, &value)) return false;
  JS_SetPendingException(cx, value);return false;
}
bool CreateDerivedException(JSContext* cx, JS::HandleObject prototype, JS::HandleString message,
                            const char* name, JS::HandleValue data, JS::MutableHandleValue result) {
  std::u16string text, label;
  JS::RootedValue raw(cx, JS::StringValue(message));
  if (!String(cx, raw, &text, false)) return false;
  Normalize(text);  // The same DOMString storage rule as the DOMException constructor.
  for (; *name; ++name) label.push_back(char16_t(static_cast<unsigned char>(*name)));
  if (!CreateException(cx, prototype, text, label, result)) return false;
  JS::SetReservedSlot(&result.toObject(), 3, data);
  return true;
}
bool DerivedExceptionData(JSObject* object, JS::MutableHandleValue data) {
  if (!object || JS::GetClass(object) != &kExceptionClass) return false;
  data.set(JS::GetReservedSlot(object, 3));
  return true;
}
bool ExceptionConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return TypeError(cx, "DOMException requires new");
  std::u16string message, name = u"Error";
  if ((!args.get(0).isUndefined() && !String(cx, args[0], &message, false)) ||
      (!args.get(1).isUndefined() && !String(cx, args[1], &name, false))) return false;
  Normalize(message);Normalize(name);
  JS::RootedObject prototype(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  return trace.Finish(CreateException(cx, prototype, message, name, args.rval(), &args));
}
bool ExceptionGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.thisv().isObject() || JS::GetClass(&args.thisv().toObject()) != &kExceptionClass)
    return TypeError(cx, "DOMException receiver is incompatible");
  const auto slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  args.rval().set(JS::GetReservedSlot(&args.thisv().toObject(), slot));return true;
}
}
