// Prototype own-key order of every installed WebIDL interface, by the donor's
// generated bindings (tools/binding_order.py -> firefox_binding_order.h). Runs once
// per realm after its owners installed; owners keep defining their members, this
// pass only re-defines them, unchanged, in Firefox's order (docs/owners/TOP_LEVEL.md).
#include "runtime/binding_order.h"
#include "firefox_binding_order.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Symbol.h"
#include <cstring>
#include <string>
#include <unordered_map>

namespace ruyijs {
namespace {
const BindingOrderEntry* Lookup(const std::string& name) {
  static const std::unordered_map<std::string, const BindingOrderEntry*> index = [] {
    std::unordered_map<std::string, const BindingOrderEntry*> map;
    for (const auto& entry : kBindingOrders) map.emplace(entry.name, &entry);
    return map;
  }();
  auto found = index.find(name);
  return found == index.end() ? nullptr : found->second;
}
// True when the string keys present in both already follow the table.
bool InOrder(JSContext* cx, JS::HandleObject prototype, const BindingOrderEntry& entry, bool* ordered) {
  JS::RootedIdVector keys(cx);
  if (!js::GetPropertyKeys(cx, prototype, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &keys)) return false;
  size_t next = 0;
  *ordered = true;
  for (size_t i = 0; i < keys.length(); ++i) {
    // Symbols compare as "@@<name>" ("Symbol.iterator" -> "@@iterator").
    JS::RootedString name(cx, keys[i].isString() ? keys[i].toString()
        : JS::GetSymbolDescription(JS::RootedSymbol(cx, keys[i].toSymbol())));
    if (!name) continue;
    JS::UniqueChars raw = JS_EncodeStringToUTF8(cx, name);
    if (!raw) return false;
    std::string label = raw.get();
    if (keys[i].isSymbol()) {
      if (label.rfind("Symbol.", 0) != 0) continue;
      label = "@@" + label.substr(7);
    }
    const char* text = label.c_str();
    size_t at = next;
    while (at < entry.count && std::strcmp(entry.members[at], text)) ++at;
    if (at < entry.count) { next = at + 1; continue; }
    // Not at or after the last placed member: either earlier (out of order) or unknown.
    for (size_t j = 0; j < next; ++j)
      if (!std::strcmp(entry.members[j], text)) { *ordered = false; return true; }
  }
  return true;
}
// WebIDL constants are read-only and permanent; owners define the prototype copy
// configurable so this pass can place it after the operations and attributes.
bool SealConstants(JSContext* cx, JS::HandleObject prototype, const BindingOrderEntry& entry) {
  for (size_t i = entry.constants; i < entry.count && entry.constants; ++i) {
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> desc(cx);
    if (!JS_GetOwnPropertyDescriptor(cx, prototype, entry.members[i], &desc)) return false;
    if (desc.isNothing() || !desc->configurable() || !desc->hasValue()) continue;
    JS::RootedValue value(cx, desc->value());
    if (!JS_DefineProperty(cx, prototype, entry.members[i], value,
                           JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) return false;
  }
  return true;
}
}  // namespace

bool OrderInterfacePrototypes(JSContext* cx, JS::HandleObject global) {
  // Every read and re-definition runs in the global's realm (see OrderOwnKeys):
  // the main page is ordered from the controller realm by runtime.cpp.
  JSAutoRealm realm(cx, global);
  JS::RootedIdVector names(cx);
  if (!js::GetPropertyKeys(cx, global, JSITER_OWNONLY | JSITER_HIDDEN, &names)) return false;
  JS::RootedValue value(cx), prototype(cx);
  for (size_t i = 0; i < names.length(); ++i) {
    if (!names[i].isString()) continue;
    JS::RootedString name(cx, names[i].toString());
    JS::UniqueChars text = JS_EncodeStringToUTF8(cx, name);
    if (!text) return false;
    const BindingOrderEntry* entry = Lookup(text.get());
    if (!entry) continue;
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> desc(cx);
    JS::RootedId id(cx, names[i]);
    if (!JS_GetOwnPropertyDescriptorById(cx, global, id, &desc)) return false;
    if (desc.isNothing() || !desc->hasValue() || !desc->value().isObject()) continue;
    JS::RootedObject constructor(cx, &desc->value().toObject());
    if (!JS_ObjectIsFunction(constructor)) continue;
    if (!JS_GetProperty(cx, constructor, "prototype", &prototype)) return false;
    if (!prototype.isObject()) continue;
    JS::RootedObject target(cx, &prototype.toObject());
    bool ordered = false;
    if (!InOrder(cx, target, *entry, &ordered)) return false;
    if (!ordered && !OrderOwnKeys(cx, target, entry->members, entry->count)) return false;
    if (!SealConstants(cx, target, *entry)) return false;
  }
  return true;
}
}  // namespace ruyijs
