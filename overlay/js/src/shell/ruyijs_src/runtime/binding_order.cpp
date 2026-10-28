#include "runtime/binding_order.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include <cstring>
#include <string>
#include <vector>
#include "js/Symbol.h"

namespace ruyijs {
namespace {
bool Redefine(JSContext* cx, JS::HandleObject target, JS::HandleId id) {
  JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> desc(cx);
  if (!JS_GetOwnPropertyDescriptorById(cx, target, id, &desc)) return false;
  if (desc.isNothing() || !desc->configurable()) return true;
  JS::Rooted<JS::PropertyDescriptor> copy(cx, *desc);
  JS::ObjectOpResult removed;
  return JS_DeletePropertyById(cx, target, id, removed) && removed.ok() &&
         JS_DefinePropertyById(cx, target, id, copy);
}
}  // namespace

bool OrderOwnKeys(JSContext* cx, JS::HandleObject target, const char* const* order, size_t count) {
  // Re-defining a property allocates the object's new shape (and, through
  // Watchtower's shadowed-property reshape, new shapes for its prototypes) in
  // the context's current zone. The caller may still be in the controller
  // realm, so enter the target's own realm: a shape from another zone is a
  // cross-zone edge that a per-zone GC sweeps while the object is alive.
  JSAutoRealm realm(cx, target);
  JS::RootedIdVector keys(cx);
  if (!js::GetPropertyKeys(cx, target, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &keys)) return false;
  std::vector<bool> placed(keys.length(), false);
  // "@@iterator" names the well-known symbol (its description is "Symbol.iterator").
  auto named = [&](size_t index, const char* name, bool* match) {
    *match = false;
    if (keys[index].isSymbol()) {
      if (std::strncmp(name, "@@", 2)) return true;
      JSString* description = JS::GetSymbolDescription(JS::RootedSymbol(cx, keys[index].toSymbol()));
      const std::string wanted = std::string("Symbol.") + (name + 2);
      return !description || JS_StringEqualsAscii(cx, description, wanted.c_str(), match);
    }
    return !keys[index].isString() || JS_StringEqualsAscii(cx, keys[index].toString(), name, match);
  };
  JS::RootedId id(cx);
  for (size_t n = 0; n < count; ++n) {
    for (size_t i = 0; i < keys.length(); ++i) {
      bool match = false;
      if (!named(i, order[n], &match)) return false;
      if (!match) continue;
      placed[i] = true;
      id = keys[i];
      if (!Redefine(cx, target, id)) return false;
      break;
    }
  }
  for (int pass = 0; pass < 3; ++pass) {
    for (size_t i = 0; i < keys.length(); ++i) {
      if (placed[i]) continue;
      bool constructor = false;
      if (!named(i, "constructor", &constructor)) return false;
      const bool symbol = keys[i].isSymbol();
      if ((pass == 0 && (constructor || symbol)) || (pass == 1 && !constructor) || (pass == 2 && !symbol)) continue;
      placed[i] = true;
      id = keys[i];
      if (!Redefine(cx, target, id)) return false;
    }
  }
  return true;
}
}  // namespace ruyijs
