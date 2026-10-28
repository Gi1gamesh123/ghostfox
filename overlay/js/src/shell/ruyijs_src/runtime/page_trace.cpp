#include "runtime/page_helpers.h"
#include <string>
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Realm.h"
#include "js/String.h"

namespace ruyijs {
bool PageHelpers::Member(JS::HandleObject function, const char* iface,
                           JS::HandleString name, const char* type) {
  if (!Enabled()) return true;
  JSAutoRealm realm(cx_, page_);
  JS::RootedString interface(cx_, JS_NewStringCopyZ(cx_, iface));
  JS::RootedString kind(cx_, JS_NewStringCopyZ(cx_, type));
  if (!interface || !kind) return false;
  JS::RootedValueArray<4> args(cx_);
  args[0].setObject(*function);
  args[1].setString(interface);
  args[2].setString(name);
  args[3].setString(kind);
  JS::RootedValue ignored(cx_);
  return Call("registerMember", args, &ignored);
}
bool PageHelpers::Member(JS::HandleObject function, const char* iface,
                           const char* name, const char* type) {
  if (!Enabled()) return true;
  JSAutoRealm realm(cx_, page_);
  JS::RootedString member(cx_, JS_NewStringCopyZ(cx_, name));
  return member && Member(function, iface, member, type);
}
bool PageHelpers::Interface(const char* iface, JS::HandleObject target) {
  if (!Enabled()) return true;
  JSAutoRealm realm(cx_, page_);
  JS::RootedString name(cx_, JS_NewStringCopyZ(cx_, iface));
  if (!name) return false;
  JS::RootedValueArray<2> args(cx_);
  args[0].setString(name);
  args[1].setObject(*target);
  JS::RootedValue ignored(cx_);
  return Call("registerInterface", args, &ignored);
}
bool PageHelpers::Object(const char* iface, JS::HandleObject target) {
  if (!Enabled()) return true;
  JSAutoRealm realm(cx_, page_);
  JS::RootedString name(cx_, JS_NewStringCopyZ(cx_, iface));
  if (!name) return false;
  JS::RootedValueArray<2> args(cx_);
  args[0].setObject(*target);
  args[1].setString(name);
  JS::RootedValue ignored(cx_);
  return Call("registerObject", args, &ignored);
}
bool PageHelpers::Accessor(const char* iface, JS::HandleObject target, const char* name) {
  if (!Enabled()) return true;
  JSAutoRealm realm(cx_, page_);
  JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> descriptor(cx_);
  if (!JS_GetOwnPropertyDescriptor(cx_, target, name, &descriptor)) return false;
  if (descriptor.isNothing() || !descriptor->isAccessorDescriptor()) {
    JS_ReportErrorASCII(cx_, "Trace registration requires an installed own accessor");
    return false;
  }
  JS::RootedObject function(cx_, descriptor->getter());
  if (function && !Member(function, iface, (std::string("get ") + name).c_str(), "get")) return false;
  function = descriptor->setter();
  return !function || Member(function, iface, (std::string("set ") + name).c_str(), "set");
}
bool PageHelpers::Constructor(const char* iface, JS::HandleObject function) {
  return Member(function, iface, iface, "call");
}
}
