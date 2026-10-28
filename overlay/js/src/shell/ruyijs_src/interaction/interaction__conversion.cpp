#include "interaction/internal.h"
#include <cstring>
#include <string>
#include "runtime/web_kit.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/GlobalObject.h"
#include "js/PropertyAndElement.h"
#include "js/StableStringChars.h"
#include "js/String.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"

namespace ruyijs::interaction {
bool Error(JSContext* cx, const char* name, const char* message) {
  JSExnType type = !strcmp(name, "TypeError") ? JSEXN_TYPEERR :
                   !strcmp(name, "RangeError") ? JSEXN_RANGEERR : JSEXN_ERR;
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message));
  JS::RootedString file(cx, JS_NewStringCopyZ(cx, "ruyijs:interaction"));
  JS::Rooted<mozilla::Maybe<JS::Value>> cause(cx, mozilla::Nothing());
  JS::RootedValue value(cx);
  if (!text || !file || !JS::CreateError(cx, type, nullptr, file, 1,
      JS::ColumnNumberOneOrigin(), nullptr, text, cause, &value)) return false;
  if (type == JSEXN_ERR) {
    JS::RootedObject object(cx, &value.toObject());
    JS::RootedString label(cx, JS_NewStringCopyZ(cx, name));
    if (!label || !JS_DefineProperty(cx, object, "name", label, 0)) return false;
  }
  JS_SetPendingException(cx, value);
  return false;
}
bool Text(JSContext* cx, JS::HandleValue value, JS::MutableHandleString text, std::string* key) {
  text.set(JS::ToString(cx, value));
  if (!text) return false;
  if (JS_GetStringLength(text) > 1024 * 1024)
    return Error(cx, "RangeError", "Event type exceeds limit");
  JS::AutoStableStringChars chars(cx);
  if (!chars.initTwoByte(cx, text)) return false;
  key->assign(reinterpret_cast<const char*>(chars.twoByteChars()), chars.length() * sizeof(char16_t));
  return true;
}
bool Option(JSContext* cx, JS::HandleObject object, const char* name, bool* value) {
  JS::RootedValue input(cx);
  if (!JS_GetProperty(cx, object, name, &input)) return false;
  *value = JS::ToBoolean(input);
  return true;
}
bool Tag(JSContext* cx, JS::HandleObject object, const char* value) {
  JS::RootedId id(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, value));
  return text && JS_DefinePropertyById(cx, object, id, text, JSPROP_READONLY);
}
Event* Receiver(JSContext* cx, const JS::CallArgs& args) {
  return Receiver(cx, args, Iface::Event);
}
Event* Receiver(JSContext* cx, const JS::CallArgs& args, Iface required, bool cross_realm) {
  JS::RootedObject object(cx, args.thisv().isObject() ? &args.thisv().toObject() : nullptr);
  if (object && cross_realm) object = js::CheckedUnwrapStatic(object);
  State* state = object ? Owner(cx, object, true) : nullptr;
  Event* event = state ? state->event(object) : nullptr;
  if (!event || !IsA(event->kind, required)) {
    // WebIDL brand failure, as Firefox words it: the member is the callee's own
    // name ("get x", "set x", "method") and the interface is the one it is on.
    std::string text;
    if (!WebKit::BrandText(cx, args, InterfaceName(required), &text)) return nullptr;
    Error(cx, "TypeError", text.c_str());
    return nullptr;
  }
  return event;
}
Target* TargetReceiver(JSContext* cx, const JS::CallArgs& args,
                       JS::MutableHandleObject object, State** owner) {
  if (args.thisv().isObject()) {
    object.set(&args.thisv().toObject());
  } else if (args.thisv().isNullOrUndefined()) {
    // Donor MaybeGlobalThisPolicy::ExtractThisObject: a bare call or an
    // explicit null/undefined this resolves to the callee realm's global
    // object (dom/bindings/BindingUtils.cpp:2863-2874), which Codegen.py:3062
    // selects for every interface on the global prototype chain - Window and
    // therefore its ancestor EventTarget (parser/WebIDL.py:1643-1654). The
    // target lookup below still decides whether that global is registered, so
    // a realm whose global is not an EventTarget fails like the donor's
    // interface check, and every other primitive this value is rejected.
    object.set(JS::GetNonCCWObjectGlobal(&args.callee()));
  }
  State* state = object ? Owner(cx, object, false) : nullptr;
  Target* target = state ? state->target(object) : nullptr;
  if (!target) {
    Error(cx, "TypeError", "EventTarget receiver is incompatible");
    return nullptr;
  }
  *owner = state;
  return target;
}
}
