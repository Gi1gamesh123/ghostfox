// FontFaceSet.load(font, text) and check(font, text): FontFaceSet::Load/Check in
// Firefox share one rule, the faces of the set whose family a CSS font shorthand
// names. load() loads them (FontFace.load semantics, settled synchronously here
// because sources are in-memory) and resolves with them in set order, or rejects
// with the first failure; check() is true when every matched face is "loaded".
// A value that is not a shorthand (no size + family, system keywords included)
// rejects / throws SyntaxError. System fonts are never set members, so a page
// font installed only system-wide matches nothing: load resolves to [] and
// check is true. A wrong receiver rejects load's promise rather than throwing.
#include "font_loading/internal.h"

#include <cctype>
#include <string>
#include <vector>

#include "js/Array.h"
#include "js/GCVector.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "jsapi.h"

namespace ruyijs::font_loading {
namespace {
std::string Lower(std::string text) {
  for (char& c : text) c = char(std::tolower(static_cast<unsigned char>(c)));
  return text;
}
std::string Trim(const std::string& text) {
  size_t a = text.find_first_not_of(" \t\n\r\f"), b = text.find_last_not_of(" \t\n\r\f");
  return a == std::string::npos ? std::string() : text.substr(a, b - a + 1);
}
bool IsSize(const std::string& token) {
  static const char* const kKeywords[] = {"xx-small", "x-small", "small", "medium", "large",
                                          "x-large", "xx-large", "xxx-large", "larger",
                                          "smaller"};
  const std::string size = Lower(token.substr(0, token.find('/')));
  for (const char* keyword : kKeywords)
    if (size == keyword) return true;
  size_t digits = 0;
  while (digits < size.size() && (std::isdigit(static_cast<unsigned char>(size[digits])) ||
                                  size[digits] == '.')) ++digits;
  if (!digits) return false;
  static const char* const kUnits[] = {"px", "pt", "pc", "em", "rem", "ex", "ch", "%", "in",
                                       "cm", "mm", "q", "vw", "vh", "vmin", "vmax", "lh"};
  for (const char* unit : kUnits)
    if (size.substr(digits) == unit) return true;
  return false;
}
// The family list of a CSS font shorthand, lower-cased and unquoted; false when
// the value is not a font shorthand (no size followed by a family).
bool Families(const std::string& font, std::vector<std::string>* out) {
  size_t at = 0;
  while (at < font.size()) {
    while (at < font.size() && std::isspace(static_cast<unsigned char>(font[at]))) ++at;
    size_t end = at;
    while (end < font.size() && !std::isspace(static_cast<unsigned char>(font[end]))) ++end;
    if (at == end) break;
    if (IsSize(font.substr(at, end - at))) {
      std::string rest = font.substr(end), item;
      for (size_t i = 0; i <= rest.size(); ++i) {
        if (i == rest.size() || rest[i] == ',') {
          item = Trim(item);
          if (item.size() >= 2 && (item.front() == '"' || item.front() == '\'') &&
              item.back() == item.front())
            item = item.substr(1, item.size() - 2);
          if (item.empty()) return false;
          out->push_back(Lower(item));
          item.clear();
        } else {
          item.push_back(rest[i]);
        }
      }
      return !out->empty();
    }
    at = end;
  }
  return false;
}

// The set's faces the shorthand names, in set order; false only on a JS error.
bool Matching(JSContext* cx, JS::HandleObject set, const std::vector<std::string>& families,
              JS::MutableHandleObjectVector out) {
  JS::RootedObject faces(cx, &JS::GetReservedSlot(set, kSetFaces).toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, faces, &length)) return false;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue item(cx);
    if (!JS_GetElement(cx, faces, i, &item)) return false;
    if (!item.isObject() || !IsFontFace(&item.toObject())) continue;
    JS::RootedString family(cx, JS::GetReservedSlot(&item.toObject(), kFontFamily).toString());
    JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, family);
    if (!bytes) return false;
    std::string name = Lower(Trim(bytes.get()));
    if (name.size() >= 2 && (name.front() == '"' || name.front() == '\'') &&
        name.back() == name.front())
      name = name.substr(1, name.size() - 2);
    for (const auto& want : families) {
      if (want != name) continue;
      if (!out.append(&item.toObject())) return false;
      break;
    }
  }
  return true;
}
bool ReadFamilies(JSContext* cx, const JS::CallArgs& args, std::vector<std::string>* families,
                  bool* valid) {
  JS::RootedString font(cx, JS::ToString(cx, args.get(0)));
  if (!font) return false;
  if (args.length() > 1 && !args.get(1).isUndefined() && !JS::ToString(cx, args.get(1)))
    return false;
  JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, font);
  if (!bytes) return false;
  *valid = Families(bytes.get(), families);
  return true;
}
}  // namespace

bool Load(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise) return false;
  args.rval().setObject(*promise);
  JS::RootedObject self(cx);
  std::vector<std::string> families;
  bool valid = false;
  if (!SetReceiver(cx, args, &self) || !ReadFamilies(cx, args, &families, &valid)) {
    // A Promise-returning operation reports its argument errors as rejection.
    JS::RootedValue error(cx);
    if (!JS_GetPendingException(cx, &error)) return false;
    JS_ClearPendingException(cx);
    return JS::RejectPromise(cx, promise, error);
  }
  if (!valid) {
    JS::RootedValue error(cx);
    return SyntaxErrorValue(cx, &error) && JS::RejectPromise(cx, promise, error);
  }
  JS::RootedObjectVector faces(cx);
  JS::RootedObject matched(cx, JS::NewArrayObject(cx, 0));
  if (!matched || !Matching(cx, self, families, &faces)) return false;
  for (size_t i = 0; i < faces.length(); ++i) {
    JS::RootedObject face(cx, faces[i]), loading(cx);
    if (!LoadFontFace(cx, face, &loading)) return false;
    if (JS::GetPromiseState(loading) == JS::PromiseState::Rejected) {
      JS::RootedValue reason(cx, JS::GetPromiseResult(loading));
      return JS::RejectPromise(cx, promise, reason);
    }
    if (!JS_DefineElement(cx, matched, uint32_t(i), face, JSPROP_ENUMERATE)) return false;
  }
  JS::RootedValue value(cx, JS::ObjectValue(*matched));
  return JS::ResolvePromise(cx, promise, value);
}

bool Check(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  std::vector<std::string> families;
  bool valid = false;
  if (!SetReceiver(cx, args, &self) || !ReadFamilies(cx, args, &families, &valid)) return false;
  if (!valid) return Fail(cx, JSEXN_SYNTAXERR, "An invalid or illegal string was specified");
  JS::RootedObjectVector faces(cx);
  if (!Matching(cx, self, families, &faces)) return false;
  bool loaded = true;
  for (size_t i = 0; i < faces.length() && loaded; ++i) {
    JS::RootedString status(cx, JS::GetReservedSlot(faces[i], kFontStatus).toString());
    if (!JS_StringEqualsAscii(cx, status, "loaded", &loaded)) return false;
  }
  args.rval().setBoolean(loaded);
  return true;
}
}  // namespace ruyijs::font_loading
