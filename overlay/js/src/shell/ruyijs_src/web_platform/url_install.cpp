// The URL installer. URL is realm-local: the constructor, the prototype and
// every accessor are created on the installing realm's global, and each member
// goes through the donor trace registry so observation sees the same surface.
#include "web_platform/state.h"

#include "runtime/page_helpers.h"
#include "js/Realm.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"

namespace ruyijs::web_platform {
namespace {
struct FieldDef {
  const char* name;
  int32_t slot;
  bool writable;
};

// href and the ten component accessors are read/write; origin and
// searchParams are read-only, matching the URL interface the donor installs.
constexpr FieldDef kFields[] = {
    {"href", kHref, true},      {"protocol", kProtocol, true},
    {"username", kUsername, true}, {"password", kPassword, true},
    {"host", kHost, true},      {"hostname", kHostname, true},
    {"port", kPort, true},      {"pathname", kPathname, true},
    {"search", kSearch, true},  {"hash", kHash, true},
    {"origin", kOrigin, false}, {"searchParams", kSearchParams, false}};
}  // namespace

bool InstallUrl(State& state, PageHelpers& helpers) {
  JSContext* cx = state.cx;
  WebKit& kit = state.kit;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  if (!objectProto) return false;
  if (!kit.Define("URL", &kUrlClass, objectProto, UrlConstructor, &state.url) ||
      !JS_DefineProperty(cx, state.url.constructor, "length", 1, JSPROP_READONLY) ||
      !kit.Method(state.url, "URL", "toString", UrlGetter, 0, kHref) ||
      !kit.Method(state.url, "URL", "toJSON", UrlGetter, 0, kHref))
    return false;
  for (const FieldDef& field : kFields) {
    JSNative setter = field.writable ? UrlSetter : nullptr;
    if (!kit.Accessor(state.url, "URL", field.name, UrlGetter, setter, field.slot))
      return false;
  }
  // Static operations in the donor's interface order: parse, canParse,
  // createObjectURL, revokeObjectURL.
  struct StaticDef {
    const char* name;
    JSNative native;
  };
  constexpr StaticDef kStatics[] = {{"parse", UrlParse}, {"canParse", UrlCanParse},
                                    {"createObjectURL", CreateObjectUrl},
                                    {"revokeObjectURL", RevokeObjectUrl}};
  for (const StaticDef& operation : kStatics) {
    JS::RootedObject function(cx, kit.Function(operation.native, operation.name, 1, 0));
    if (!function ||
        !JS_DefineProperty(cx, state.url.constructor, operation.name, function, JSPROP_ENUMERATE) ||
        !helpers.Member(function, "URL", operation.name, "call"))
      return false;
  }
  return true;
}

// webkitURL is the same interface object, not a copy: Window globals only
// (URL.webidl LegacyWindowAlias), with the interface-object attributes.
bool InstallUrlWindowAlias(State& state) {
  return JS_DefineProperty(state.cx, state.kit.page, "webkitURL", state.url.constructor, 0);
}
}  // namespace ruyijs::web_platform
