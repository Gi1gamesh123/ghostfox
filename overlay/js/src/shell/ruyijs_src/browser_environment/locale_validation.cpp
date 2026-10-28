#include "browser_environment/state.h"
#include "runtime/runtime.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/RealmOptions.h"
#include "js/String.h"
#include "js/ValueArray.h"

namespace ruyijs::browser_environment {
static bool CanonicalLocale(JSContext* cx, JS::HandleObject intl, std::string* value) {
  JS::RootedString source(cx, JS_NewStringCopyUTF8N(cx,
      JS::UTF8Chars(value->data(), value->size())));
  if (!source) return false;
  JS::RootedValueArray<1> arguments(cx);
  arguments[0].setString(source);
  JS::RootedValue canonical(cx), first(cx);
  if (!JS_CallFunctionName(cx, intl, "getCanonicalLocales", arguments, &canonical)) return false;
  JS::RootedObject options(cx, JS_NewPlainObject(cx));
  JS::RootedString matcher(cx, JS_NewStringCopyZ(cx, "lookup"));
  if (!options || !matcher ||
      !JS_DefineProperty(cx, options, "localeMatcher", matcher, JSPROP_ENUMERATE)) return false;
  JS::RootedValueArray<2> supportedArguments(cx);
  supportedArguments[0].set(canonical);
  supportedArguments[1].setObject(*options);
  // Native DefaultLocale requires a lookup match in both ICU locale sets.
  const char* constructors[] = {"DateTimeFormat", "Collator"};
  for (const char* name : constructors) {
    JS::RootedValue constructor(cx), supported(cx);
    if (!JS_GetProperty(cx, intl, name, &constructor)) return false;
    JS::RootedObject service(cx, &constructor.toObject());
    if (!JS_CallFunctionName(cx, service, "supportedLocalesOf", supportedArguments, &supported))
      return false;
    JS::RootedObject list(cx, &supported.toObject());
    uint32_t count;
    if (!JS::GetArrayLength(cx, list, &count)) return false;
    if (!count) return Error(cx, "Locale is not supported by all runtime Intl services", JSEXN_RANGEERR);
  }
  JS::RootedObject names(cx, &canonical.toObject());
  if (!JS_GetElement(cx, names, 0, &first)) return false;
  JS::RootedString text(cx, first.toString());
  return ToUTF8(cx, text, value);
}
static bool CanonicalTimezone(JSContext* cx, JS::HandleValue constructor, std::string* value) {
  if (value->empty() || (*value)[0] == '+' || (*value)[0] == '-')
    return Error(cx, "Timezone must be a named IANA identifier", JSEXN_RANGEERR);
  JS::RootedString source(cx, JS_NewStringCopyUTF8N(cx,
      JS::UTF8Chars(value->data(), value->size())));
  JS::RootedObject options(cx, JS_NewPlainObject(cx)), formatter(cx);
  if (!source || !options ||
      !JS_DefineProperty(cx, options, "timeZone", source, JSPROP_ENUMERATE)) return false;
  JS::RootedValueArray<2> arguments(cx);
  arguments[0].setUndefined();
  arguments[1].setObject(*options);
  if (!JS::Construct(cx, constructor, arguments, &formatter)) return false;
  JS::RootedValue resolved(cx), name(cx);
  if (!JS_CallFunctionName(cx, formatter, "resolvedOptions", JS::HandleValueArray::empty(),
                             &resolved)) return false;
  JS::RootedObject settings(cx, &resolved.toObject());
  if (!JS_GetProperty(cx, settings, "timeZone", &name)) return false;
  JS::RootedString text(cx, name.toString());
  return ToUTF8(cx, text, value);
}
bool State::ValidateLocale(std::vector<Change>* changes) {
  if (!controller) return Error(cx, "Locale validation realm is missing");
  JSAutoRealm realm(cx, controller);
  JS::RootedValue intlValue(cx), constructor(cx);
  if (!JS_GetProperty(cx, controller, "Intl", &intlValue)) return false;
  JS::RootedObject intl(cx, &intlValue.toObject());
  if (!JS_GetProperty(cx, intl, "DateTimeFormat", &constructor)) return false;
  for (auto& change : *changes) {
    bool locale = change.kind == Kind::Navigator && change.field == 9;
    bool timezone = change.kind == Kind::Locale && change.field == 0;
    if (!locale && !timezone) continue;
    auto& text = change.value.text;
    if (text.size() > 255 || text.find('\0') != std::string::npos)
      return Error(cx, "Locale or timezone identifier is invalid", JSEXN_RANGEERR);
    if (locale && !CanonicalLocale(cx, intl, &text)) return false;
    if (timezone && !CanonicalTimezone(cx, constructor, &text)) return false;
  }
  return true;
}
void State::ApplyLocale(const std::vector<Change>& changes) {
  JS::Realm* realm = JS::GetObjectRealmOrNull(page);
  for (const auto& change : changes) {
    if (change.kind == Kind::Navigator && change.field == 9)
      JS::SetRealmLocaleOverride(realm, change.value.text.c_str());
    if (change.kind == Kind::Locale && change.field == 0)
      JS::SetRealmTimezoneOverride(realm, change.value.text.c_str());
  }
}
}
