#include "font_loading/internal.h"

#include "runtime/page_helpers.h"
#include "js/Conversions.h"
#include "js/CharacterEncoding.h"
#include "jsfriendapi.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <cstdlib>
#include <string>

namespace ruyijs::font_loading {
namespace {

// The CSS descriptors stored by FontFace. Validation is deliberately lexical:
// this runtime has no styling engine, so it keeps the declared value and the
// spec's rejection of malformed input instead of resolving computed styles.
enum class DescriptorKind { Style, Weight, Stretch, UnicodeRange, FeatureSettings, Variant, Display };

struct Descriptor {
  const char* name;
  int slot;
  const char* fallback;
  DescriptorKind kind;
};

const Descriptor kDescriptors[] = {
    {"style", kFontStyle, "normal", DescriptorKind::Style},
    {"weight", kFontWeight, "normal", DescriptorKind::Weight},
    {"stretch", kFontStretch, "normal", DescriptorKind::Stretch},
    {"unicodeRange", kFontUnicodeRange, "U+0-10FFFF", DescriptorKind::UnicodeRange},
    {"featureSettings", kFontFeatureSettings, "normal", DescriptorKind::FeatureSettings},
    {"variant", kFontVariant, "normal", DescriptorKind::Variant},
    {"display", kFontDisplay, "auto", DescriptorKind::Display},
};

std::string Lower(std::string value) {
  for (char& c : value) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
  return value;
}

bool In(std::string value, std::initializer_list<const char*> set) {
  value = Lower(std::move(value));
  for (const char* item : set) if (value == item) return true;
  return false;
}

bool Digits(const std::string& value) {
  if (value.empty()) return false;
  for (char c : value) if (c < '0' || c > '9') return false;
  return true;
}

bool Percent(const std::string& value) {
  if (value.size() < 2 || value.back() != '%') return false;
  bool dot = false;
  for (size_t i = 0; i + 1 < value.size(); ++i) {
    char c = value[i];
    if (c == '.' && !dot) { dot = true; continue; }
    if (c < '0' || c > '9') return false;
  }
  return true;
}

bool HexOrWild(const std::string& value, size_t limit) {
  if (value.empty() || value.size() > limit) return false;
  for (char c : value)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || c == '?')) return false;
  return true;
}

bool UnicodeToken(const std::string& token) {
  if (token.size() < 3 || token[0] != 'u' || token[1] != '+') return false;
  const size_t dash = token.find('-', 2);
  const std::string head = token.substr(2, dash == std::string::npos ? std::string::npos : dash - 2);
  if (!HexOrWild(head, 6)) return false;
  if (dash == std::string::npos) return true;
  const std::string tail = token.substr(dash + 1);
  return !tail.empty() && tail.size() <= 6 && HexOrWild(tail, 6);
}

bool UnicodeList(const std::string& value) {
  size_t start = 0;
  while (true) {
    const size_t comma = value.find(',', start);
    std::string token = value.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
    while (!token.empty() && token.front() == ' ') token.erase(token.begin());
    while (!token.empty() && token.back() == ' ') token.pop_back();
    if (!UnicodeToken(Lower(token))) return false;
    if (comma == std::string::npos) return true;
    start = comma + 1;
  }
}

bool FeatureList(const std::string& value) {
  size_t start = 0;
  while (true) {
    const size_t comma = value.find(',', start);
    std::string token = value.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
    while (!token.empty() && token.front() == ' ') token.erase(token.begin());
    while (!token.empty() && token.back() == ' ') token.pop_back();
    if (token.size() < 3 || token.front() != '"') return false;
    const size_t quote = token.find('"', 1);
    if (quote == std::string::npos || quote == 1 || quote > 5) return false;
    const std::string tail = token.substr(quote + 1);
    if (!tail.empty()) {
      if (tail.front() != ' ') return false;
      std::string setting = tail.substr(1);
      if (setting != "on" && setting != "off" && !Digits(setting))
        return false;
    }
    if (comma == std::string::npos) return true;
    start = comma + 1;
  }
}

bool Normalize(DescriptorKind kind, const std::string& input, std::string* out) {
  switch (kind) {
    case DescriptorKind::Style:
      if (!In(input, {"normal", "italic", "oblique"})) return false;
      *out = Lower(input);
      return true;
    case DescriptorKind::Weight:
      if (In(input, {"normal", "bold"})) { *out = Lower(input); return true; }
      if (!Digits(input)) return false;
      { const long weight = std::strtol(input.c_str(), nullptr, 10);
        if (weight < 1 || weight > 1000) return false; }
      *out = input;
      return true;
    case DescriptorKind::Stretch:
      if (In(input, {"ultra-condensed", "extra-condensed", "condensed", "semi-condensed",
                     "normal", "semi-expanded", "expanded", "extra-expanded", "ultra-expanded"})) {
        *out = Lower(input);
        return true;
      }
      if (!Percent(input)) return false;
      *out = input;
      return true;
    case DescriptorKind::UnicodeRange:
      if (!UnicodeList(input)) return false;
      *out = input;
      return true;
    case DescriptorKind::FeatureSettings:
      if (Lower(input) == "normal") { *out = "normal"; return true; }
      if (!FeatureList(input)) return false;
      *out = input;
      return true;
    case DescriptorKind::Variant:
      if (!In(input, {"normal", "none", "small-caps", "all-small-caps", "petite-caps",
                      "all-petite-caps", "unicase", "titling-caps", "common-ligatures",
                      "no-common-ligatures", "discretionary-ligatures",
                      "no-discretionary-ligatures", "historical-ligatures",
                      "no-historical-ligatures", "contextual", "no-contextual"})) return false;
      *out = Lower(input);
      return true;
    case DescriptorKind::Display:
      if (!In(input, {"auto", "block", "swap", "fallback", "optional"})) return false;
      *out = Lower(input);
      return true;
  }
  return false;
}

const Descriptor* Find(const std::string& name) {
  for (const Descriptor& descriptor : kDescriptors)
    if (name == descriptor.name) return &descriptor;
  return nullptr;
}

bool Store(JSContext* cx, JS::HandleObject face, const Descriptor& descriptor,
           const std::string& text) {
  JS::RootedString value(cx, JS_NewStringCopyZ(cx, text.c_str()));
  if (!value) return false;
  JS::SetReservedSlot(face, descriptor.slot, JS::StringValue(value));
  return true;
}

bool DescriptorGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false;
  const int index = js::GetFunctionNativeReserved(&args.callee(), 0).toInt32();
  args.rval().set(JS::GetReservedSlot(self, kDescriptors[index].slot));
  return true;
}

bool DescriptorSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false;
  // callee() must be read before the return value is touched: the two share a
  // slot, so reading it afterwards would interpret the rval as a function.
  const int index = js::GetFunctionNativeReserved(&args.callee(), 0).toInt32();
  args.rval().setUndefined();
  if (args.length() == 0) return true;
  const Descriptor& descriptor = kDescriptors[index];
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  if (!text) return false;
  JS::UniqueChars encoded = JS_EncodeStringToUTF8(cx, text);
  if (!encoded) return false;
  std::string normalized;
  if (!Normalize(descriptor.kind, std::string(encoded.get()), &normalized))
    return Fail(cx, JSEXN_SYNTAXERR, "Invalid FontFace descriptor value");
  return Store(cx, self, descriptor, normalized);
}

}  // namespace

const char* const kFontDescriptorNames[7] = {
    "style", "weight", "stretch", "unicodeRange", "featureSettings", "variant", "display"};

bool InstallFontFaceAttributes(JSContext* cx, JS::HandleObject prototype, PageHelpers& helpers) {
  for (size_t index = 0; index < 7; ++index) {
    const Descriptor& descriptor = kDescriptors[index];
    JS::RootedFunction getter(cx, js::NewFunctionWithReserved(cx, DescriptorGet, 0, 0,
                                                              descriptor.name));
    JS::RootedFunction setter(cx, js::NewFunctionWithReserved(cx, DescriptorSet, 1, 0,
                                                              descriptor.name));
    if (!getter || !setter) return false;
    JS::RootedObject get_object(cx, JS_GetFunctionObject(getter));
    JS::RootedObject set_object(cx, JS_GetFunctionObject(setter));
    if (!get_object || !set_object) return false;
    js::SetFunctionNativeReserved(get_object, 0, JS::Int32Value(static_cast<int32_t>(index)));
    js::SetFunctionNativeReserved(set_object, 0, JS::Int32Value(static_cast<int32_t>(index)));
    if (!JS_DefineProperty(cx, prototype, descriptor.name, get_object, set_object,
                           JSPROP_ENUMERATE)) return false;
    JS::RootedString name(cx, JS_NewStringCopyZ(cx, descriptor.name));
    if (!name || !helpers.Member(get_object, "FontFace", name, "get") ||
        !helpers.Member(set_object, "FontFace", name, "set")) return false;
  }
  return true;
}

bool ApplyFontFaceDescriptor(JSContext* cx, JS::HandleObject face, const char* name,
                             JS::HandleValue value, bool* recognized) {
  const Descriptor* descriptor = Find(name);
  if (!descriptor) {
    if (recognized) *recognized = false;
    return true;
  }
  if (recognized) *recognized = true;
  if (value.isUndefined()) return Store(cx, face, *descriptor, descriptor->fallback);
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  JS::UniqueChars encoded = JS_EncodeStringToUTF8(cx, text);
  if (!encoded) return false;
  std::string normalized;
  if (!Normalize(descriptor->kind, std::string(encoded.get()), &normalized))
    return Fail(cx, JSEXN_SYNTAXERR, "Invalid FontFace descriptor value");
  return Store(cx, face, *descriptor, normalized);
}

}  // namespace ruyijs::font_loading
