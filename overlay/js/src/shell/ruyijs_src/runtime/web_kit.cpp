#include "runtime/web_kit.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CharacterEncoding.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/JSON.h"
#include "js/Promise.h"
#include <cstring>
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/String.h"
#include "js/Symbol.h"
#include <string>

namespace ruyijs {
WebKit::WebKit(JSContext* context, JS::HandleObject global, PageHelpers& trace, void* owner)
    : cx(context), helpers(&trace), owner(owner), page(context, global),
      functions(context, JS::GCVector<JSObject*>(context)) {}
bool WebKit::Fail(JSContext* cx, const char* message) {
  return FailKind(cx, message, JSEXN_TYPEERR);
}
bool WebKit::FailKind(JSContext* cx, const char* message, int16_t kind) {
  // Messages may quote script input (a URL), which is UTF-8.
  JS::RootedString text(cx, JS_NewStringCopyUTF8Z(cx, JS::ConstUTF8CharsZ(message, strlen(message))));
  JS::RootedString file(cx, JS_NewStringCopyZ(cx, "ruyijs:web"));
  JS::Rooted<mozilla::Maybe<JS::Value>> cause(cx, mozilla::Nothing());
  JS::RootedValue value(cx);
  if (!text || !file || !JS::CreateError(cx, JSExnType(kind), nullptr, file, 1,
      JS::ColumnNumberOneOrigin(), nullptr, text, cause, &value)) return false;
  JS_SetPendingException(cx, value);
  return false;
}
bool WebKit::BrandText(JSContext* cx, const JS::CallArgs& args, const char* iface,
                       std::string* text) {
  std::string member;
  JS::RootedFunction function(cx, JS_GetObjectFunction(&args.callee()));
  JS::RootedString id(cx);
  // JS_GetFunctionId resolves an accessor's lazy "get "/"set " prefix.
  if (function && (!JS_GetFunctionId(cx, function, &id) || (id && !ToUTF8(cx, id, &member))))
    return false;
  *text = "'" + member + "' called on an object that does not implement interface " + iface + ".";
  return true;
}
bool WebKit::FailBrand(JSContext* cx, const JS::CallArgs& args, const char* iface) {
  std::string text;
  return BrandText(cx, args, iface, &text) && Fail(cx, text.c_str());
}
bool WebKit::Reject(JSContext* cx, unsigned, JS::Value*) {
  return Fail(cx, "Illegal constructor.");
}
void* WebKit::Owner(const JS::CallArgs& args) {
  return js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate();
}
int32_t WebKit::Slot(const JS::CallArgs& args) {
  return js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
}
bool WebKit::Check(JSContext* cx, JS::HandleValue self, const JSClass* clasp,
                   JS::MutableHandleObject out, const char* message) {
  if (!self.isObject() || JS::GetClass(&self.toObject()) != clasp) return Fail(cx, message);
  out.set(&self.toObject());
  return true;
}
bool WebKit::CheckBrand(JSContext* cx, const JS::CallArgs& args, const JSClass* clasp,
                        JS::MutableHandleObject out, const char* iface) {
  JS::HandleValue self = args.thisv();
  if (!self.isObject() || JS::GetClass(&self.toObject()) != clasp)
    return FailBrand(cx, args, iface);
  out.set(&self.toObject());
  return true;
}
JSObject* WebKit::Function(JSNative native, const char* name, unsigned length,
                           int32_t slot, unsigned flags) {
  JS::RootedFunction function(cx, js::NewFunctionWithReserved(cx, native, length, flags, name));
  if (!function) return nullptr;
  JS::RootedObject object(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(owner));
  js::SetFunctionNativeReserved(object, 1, JS::Int32Value(slot));
  if (!functions.get().append(object)) { JS_ReportOutOfMemory(cx); return nullptr; }
  return object;
}
bool WebKit::Define(const char* name, const JSClass* clasp, JS::HandleObject parent,
                    JSNative ctor, WebFace* face, unsigned ctorLength) {
  face->clasp = clasp;
  face->prototype = JS_NewPlainObject(cx);
  face->constructor = Function(ctor, name, ctorLength, 0, JSFUN_CONSTRUCTOR);
  if (!face->prototype || !face->constructor ||
      (parent && !JS_SetPrototype(cx, face->prototype, parent))) return false;
  // A WebIDL interface object's own keys are length, name, prototype; resolve
  // the function's lazy length/name before defining prototype so they lead.
  JS::RootedObject constructor(cx, face->constructor);
  bool resolved = false;
  if (!JS_HasOwnProperty(cx, constructor, "length", &resolved) ||
      !JS_HasOwnProperty(cx, constructor, "name", &resolved)) return false;
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, name));
  JS::RootedId tagId(cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  if (!tag || !JS_DefineProperty(cx, face->constructor, "prototype", face->prototype,
                                 JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(cx, face->prototype, "constructor", face->constructor, 0) ||
      !JS_DefinePropertyById(cx, face->prototype, tagId, tag, JSPROP_READONLY) ||
      !JS_DefineProperty(cx, page, name, face->constructor, 0)) return false;
  return helpers->Constructor(name, face->constructor) &&
         helpers->Interface(name, face->prototype);
}
bool WebKit::Method(WebFace& face, const char* iface, const char* name, JSNative native,
                    unsigned length, int32_t slot) {
  JS::RootedObject method(cx, Function(native, name, length, slot));
  if (!method || !JS_DefineProperty(cx, face.prototype, name, method, JSPROP_ENUMERATE))
    return false;
  return helpers->Member(method, iface, name, "call");
}
bool WebKit::Accessor(WebFace& face, const char* iface, const char* name, JSNative getter,
                      JSNative setter, int32_t slot) {
  std::string getName = std::string("get ") + name;
  JS::RootedObject get(cx, Function(getter, getName.c_str(), 0, slot));
  JS::RootedObject set(cx);
  if (setter) {
    std::string setName = std::string("set ") + name;
    set = Function(setter, setName.c_str(), 1, slot);
    if (!set) return false;
  }
  if (!get || !JS_DefineProperty(cx, face.prototype, name, get, set, JSPROP_ENUMERATE))
    return false;
  return helpers->Accessor(iface, face.prototype, name);
}
bool WebKit::Resolve(JS::HandleValue value, JS::MutableHandleValue result) {
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !JS::ResolvePromise(cx, promise, value)) return false;
  result.setObject(*promise);
  return true;
}
bool WebKit::RejectWith(JS::HandleValue reason, JS::MutableHandleValue result) {
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise) return false;
  JS::RootedValue rooted(cx, reason);
  if (!JS::RejectPromise(cx, promise, rooted)) return false;
  result.setObject(*promise);
  return true;
}
JSObject* WebKit::New(const JSClass* clasp, JS::HandleObject proto) {
  return JS_NewObjectWithGivenProto(cx, clasp, proto);
}
bool WebKit::ConstructProto(const JS::CallArgs& args, JS::HandleObject fallback,
                            JS::MutableHandleObject proto) {
  if (!args.isConstructing()) return Fail(cx, "Illegal constructor.");
  JS::RootedObject target(cx, &args.newTarget().toObject());
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, target, "prototype", &value)) return false;
  if (value.isObject()) proto.set(&value.toObject());
  else proto.set(fallback);
  return true;
}
static bool AppendJson(const char16_t* data, uint32_t len, void* raw) {
  auto* text = static_cast<std::u16string*>(raw);
  if (text->size() + len > 1024 * 1024) return false;
  text->append(data, len);
  return true;
}
bool JsonText(JSContext* cx, JS::HandleValue value, std::string* utf8) {
  JS::RootedValue copy(cx, value);
  std::u16string text;
  JS::RootedObject replacer(cx);
  JS::RootedValue space(cx, JS::UndefinedValue());
  if (!JS_Stringify(cx, &copy, replacer, space, AppendJson, &text)) return false;
  JS::RootedString encoded(cx, JS_NewUCStringCopyN(cx, text.data(), text.size()));
  return encoded && ToUTF8(cx, encoded, utf8);
}
bool JsonValue(JSContext* cx, const std::string& utf8, JS::MutableHandleValue value) {
  if (utf8.size() > 1024 * 1024) {
    JS_ReportErrorASCII(cx, "JSON value exceeds the 1 MiB session limit");
    return false;
  }
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(utf8.data(), utf8.size())));
  return text && JS_ParseJSON(cx, text, value);
}
}
