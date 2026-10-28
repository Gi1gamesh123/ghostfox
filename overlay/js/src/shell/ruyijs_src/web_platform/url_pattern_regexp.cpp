// SpiderMonkey regexp evaluation for the urlpattern crate, as in Firefox
// URLPatternGlue.cpp: the `v` flag plus `i`, compiled in the calling realm.
// Each call compiles a fresh RegExp object, so no JS object outlives the call
// and dropping a Rust pattern never touches the engine.
#include "web_platform/url_pattern_ffi.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/RegExp.h"
#include "js/RegExpFlags.h"
#include "runtime/runtime.h"

namespace ruyijs::web_platform {
namespace {
thread_local JSContext* gCx = nullptr;
thread_local JSObject* gGlobal = nullptr;
std::u16string Utf16(const uint8_t* text, size_t length) {  // the crate passes valid UTF-8
  std::u16string out;
  out.reserve(length);
  for (size_t i = 0; i < length;) {
    uint32_t c = text[i];
    const uint32_t extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    c = extra == 3 ? c & 7 : extra == 2 ? c & 15 : extra == 1 ? c & 31 : c;
    for (uint32_t k = 1; k <= extra && i + k < length; ++k) c = (c << 6) | (text[i + k] & 63);
    i += extra + 1;
    if (c >= 0x10000) {
      c -= 0x10000;
      out.push_back(char16_t(0xD800 + (c >> 10)));
      out.push_back(char16_t(0xDC00 + (c & 0x3FF)));
    } else {
      out.push_back(char16_t(c));
    }
  }
  return out;
}
JSObject* Compile(JSContext* cx, const uint16_t* pattern, size_t len, bool ignore_case) {
  JS::RegExpFlags flags = JS::RegExpFlag::UnicodeSets;
  if (ignore_case) flags = flags | JS::RegExpFlag::IgnoreCase;
  return JS::NewUCRegExpObject(cx, reinterpret_cast<const char16_t*>(pattern), len, flags);
}
}  // namespace
UrlPatternRegExpScope::UrlPatternRegExpScope(JSContext* cx, JSObject* global)
    : previousCx_(gCx), previousGlobal_(gGlobal) {
  gCx = cx;
  gGlobal = global;
}
UrlPatternRegExpScope::~UrlPatternRegExpScope() {
  gCx = previousCx_;
  gGlobal = previousGlobal_;
}

// mfbt HashBytes + ScrambleHashCode and PLDHashTable double hashing
// (kMinCapacity 8, max load 75%), reproduced for Firefox's record order.
std::vector<size_t> UrlPatternGroupOrder(const std::vector<std::string>& names) {
  const auto rotl5 = [](uint32_t x) { return (x << 5) | (x >> 27); };
  const auto add = [&](uint32_t h, uint32_t v) { return uint32_t(0x9E3779B9u * (rotl5(h) ^ v)); };
  const auto keyHash = [&](const std::string& s) {
    uint32_t h = 0;
    size_t i = 0;
    const size_t n = s.size();
    for (; i < n - n % 4; i += 4)
      h = add(h, uint32_t(uint8_t(s[i])) | uint32_t(uint8_t(s[i + 1])) << 8 |
                 uint32_t(uint8_t(s[i + 2])) << 16 | uint32_t(uint8_t(s[i + 3])) << 24);
    for (; i < n; ++i) h = add(h, uint8_t(s[i]));
    uint32_t k = h * 0x9E3779B9u;
    if (k < 2) k -= 2;
    return k & ~1u;
  };
  struct Entry { uint32_t key = 0; size_t index = 0; bool used = false; };
  const auto place = [](std::vector<Entry>& table, uint32_t bits, uint32_t key, size_t index) {
    uint32_t h1 = key >> (32 - bits);
    if (table[h1].used) {
      const uint32_t mask = (1u << bits) - 1, h2 = (key & mask) | 1;
      do { h1 = (h1 - h2) & mask; } while (table[h1].used);
    }
    table[h1] = {key, index, true};
  };
  uint32_t log2 = 3;
  std::vector<Entry> slots(8);
  for (size_t i = 0; i < names.size(); ++i) {
    const uint32_t capacity = 1u << log2;
    if (i >= capacity - (capacity >> 2)) {
      std::vector<Entry> grown(size_t(capacity) * 2);
      for (const Entry& e : slots) if (e.used) place(grown, log2 + 1, e.key, e.index);
      slots.swap(grown);
      ++log2;
    }
    place(slots, log2, keyHash(names[i]), i);
  }
  std::vector<size_t> order;
  for (const Entry& e : slots) if (e.used) order.push_back(e.index);
  return order;
}
}  // namespace ruyijs::web_platform

using ruyijs::web_platform::gCx;
using ruyijs::web_platform::gGlobal;

extern "C" bool ruyijs_urlpattern_regexp_valid(const uint16_t* pattern, size_t len, bool ignore_case) {
  if (!gCx || !gGlobal) return false;
  JSAutoRealm realm(gCx, gGlobal);
  JS::RootedObject regexp(gCx, ruyijs::web_platform::Compile(gCx, pattern, len, ignore_case));
  if (!regexp) JS_ClearPendingException(gCx);
  return regexp != nullptr;
}

extern "C" bool ruyijs_urlpattern_regexp_exec(const uint16_t* pattern, size_t len, bool ignore_case,
                                              const uint8_t* text, size_t text_len, bool match_only,
                                              bool* matched, void* captures) {
  *matched = false;
  if (!gCx || !gGlobal) return false;
  JSContext* cx = gCx;
  JSAutoRealm realm(cx, gGlobal);
  JS::RootedObject regexp(cx, ruyijs::web_platform::Compile(cx, pattern, len, ignore_case));
  if (!regexp) { JS_ClearPendingException(cx); return false; }
  std::u16string input = ruyijs::web_platform::Utf16(text, text_len);
  JS::RootedValue result(cx, JS::NullValue());
  size_t index = 0;
  if (!JS::ExecuteRegExpNoStatics(cx, regexp, input.data(), input.size(), &index, match_only, &result)) {
    JS_ClearPendingException(cx);
    return false;
  }
  if (result.isNull()) return true;
  *matched = true;
  if (match_only) return true;
  JS::RootedObject array(cx, &result.toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, array, &length)) { JS_ClearPendingException(cx); return false; }
  for (uint32_t i = 1; i < length; ++i) {  // index 0 is the whole match
    JS::RootedValue element(cx);
    if (!JS_GetElement(cx, array, i, &element)) { JS_ClearPendingException(cx); return false; }
    if (element.isUndefined()) { ruyijs_urlpattern_push_capture(captures, ruyijs::web_platform::RyNone()); continue; }
    JS::RootedString string(cx, JS::ToString(cx, element));
    std::string utf8;
    if (!string || !ruyijs::ToUTF8(cx, string, &utf8)) { JS_ClearPendingException(cx); return false; }
    ruyijs_urlpattern_push_capture(captures, ruyijs::web_platform::RyOf(utf8));
  }
  return true;
}

extern "C" void ruyijs_urlpattern_sink(void* sink, RyStr value) {
  auto& values = *static_cast<ruyijs::web_platform::UrlPatternValues*>(sink);
  if (value.valid) values.emplace_back(ruyijs::web_platform::RyString(value));
  else values.emplace_back(std::nullopt);
}
