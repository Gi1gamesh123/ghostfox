// The "fonts" profile group (families / hidden / serif, sansSerif, monospace,
// cursive, fantasy). Validation is strict; a bad group leaves the previous
// configuration untouched.
#include "web_geometry/font_config.h"
#include "web_geometry/realm.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include <windows.h>
#include <map>
#include <mutex>
#include <set>

namespace ruyijs::web_geometry {
namespace {
struct Config {
  bool restrict_families = false;
  std::set<std::wstring> families, hidden;
  std::map<std::string, std::string> generics;
};
std::mutex g_lock;
Config g_config;
uint64_t g_generation = 0;

std::wstring Lower(const std::wstring& text) {
  std::wstring out = text;
  for (auto& ch : out) ch = wchar_t(towlower(ch));
  return out;
}
std::wstring Wide(const std::string& utf8) {
  const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
  std::wstring out(size, L'\0');
  if (size) MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), out.data(), size);
  return out;
}
bool Text(JSContext* cx, JS::HandleValue value, std::string* out) {
  JS::RootedString text(cx, value.toString());
  JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, text);
  if (!utf8) return false;
  *out = utf8.get();
  return true;
}
// Reads an optional string array; `present` reports whether the key was given.
bool Names(JSContext* cx, JS::HandleObject group, const char* name, bool* present,
           std::set<std::wstring>* out) {
  const std::string bad = std::string("fonts.") + name + " must be an array of strings";
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, group, name, &value)) return false;
  *present = !value.isUndefined();
  if (!*present) return true;
  bool array = false;
  uint32_t length = 0;
  JS::RootedObject list(cx);
  if (value.isObject()) {
    list = &value.toObject();
    if (!JS::IsArray(cx, list, &array)) return false;
  }
  if (!array || !JS::GetArrayLength(cx, list, &length) || length > 4096)
    return WebKit::Fail(cx, bad.c_str());
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue item(cx);
    if (!JS_GetElement(cx, list, i, &item)) return false;
    if (!item.isString()) return WebKit::Fail(cx, bad.c_str());
    std::string text;
    if (!Text(cx, item, &text)) return false;
    out->insert(Lower(Wide(text)));
  }
  return true;
}
}  // namespace

bool FontFamilyVisible(const std::wstring& family) {
  std::lock_guard<std::mutex> guard(g_lock);
  if (!g_config.restrict_families && g_config.hidden.empty()) return true;
  const auto key = Lower(family);
  if (g_config.hidden.count(key)) return false;
  return !g_config.restrict_families || g_config.families.count(key);
}
std::string GenericFamilyOverride(const std::string& generic) {
  std::lock_guard<std::mutex> guard(g_lock);
  const auto found = g_config.generics.find(generic);
  return found == g_config.generics.end() ? std::string() : found->second;
}
uint64_t FontConfigGeneration() {
  std::lock_guard<std::mutex> guard(g_lock);
  return g_generation;
}

bool ConfigureProfileGroup(void* context, JS::HandleObject group) {
  JSContext* cx = static_cast<JSContext*>(context);
  Config next;
  bool unused = false;
  if (!Names(cx, group, "families", &next.restrict_families, &next.families) ||
      !Names(cx, group, "hidden", &unused, &next.hidden)) return false;
  static const std::pair<const char*, const char*> kGenerics[] = {
      {"serif", "serif"}, {"sansSerif", "sans-serif"}, {"monospace", "monospace"},
      {"cursive", "cursive"}, {"fantasy", "fantasy"}};
  for (const auto& [key, generic] : kGenerics) {
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, group, key, &value)) return false;
    if (value.isUndefined()) continue;
    if (!value.isString())
      return WebKit::Fail(cx, (std::string("fonts.") + key + " must be a string").c_str());
    if (!Text(cx, value, &next.generics[generic])) return false;
  }
  std::lock_guard<std::mutex> guard(g_lock);
  g_config = std::move(next);
  ++g_generation;
  return true;
}
}  // namespace ruyijs::web_geometry
