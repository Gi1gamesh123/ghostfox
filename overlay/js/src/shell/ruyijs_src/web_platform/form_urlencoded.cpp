// The application/x-www-form-urlencoded codec. One implementation serves both
// URLSearchParams and (later) FormData serialization, so a name/value pair has
// a single encoding rule in the product.
#include "web_platform/state.h"
#include "domain/browser_environment/url_value.h"
#include "runtime/runtime.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/String.h"

namespace ruyijs::web_platform {
namespace {
const char kHex[] = "0123456789ABCDEF";
bool Unreserved(unsigned char unit) {
  return (unit >= '0' && unit <= '9') || (unit >= 'A' && unit <= 'Z') ||
         (unit >= 'a' && unit <= 'z') || unit == '*' || unit == '-' || unit == '.' ||
         unit == '_';
}
}  // namespace

std::string FormEncode(std::string_view text) {
  std::string out;
  for (unsigned char unit : text) {
    if (Unreserved(unit)) out.push_back(char(unit));
    else if (unit == ' ') out.push_back('+');
    else {
      out.push_back('%');
      out.push_back(kHex[unit >> 4]);
      out.push_back(kHex[unit & 0x0f]);
    }
  }
  return out;
}

std::string SerializeFormQuery(
    const std::vector<std::pair<std::string, std::string>>& entries) {
  std::string out;
  for (const auto& entry : entries) {
    if (!out.empty()) out.push_back('&');
    out += FormEncode(entry.first);
    out.push_back('=');
    out += FormEncode(entry.second);
  }
  return out;
}

bool Convert(JSContext* cx, JS::HandleValue value, std::string* text) {
  JS::RootedString string(cx, JS::ToString(cx, value));
  return string && ToUTF8(cx, string, text);
}

bool Value(JSContext* cx, std::string_view text, JS::MutableHandleValue result) {
  JS::RootedString string(
      cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size())));
  if (!string) return false;
  result.setString(string);
  return true;
}

bool ParamsEntries(JSContext* cx, JS::HandleObject params,
                   std::vector<std::pair<std::string, std::string>>* entries) {
  JS::RootedValue query(cx, JS::GetReservedSlot(params, kParamsQuerySlot));
  std::string text;
  if (!query.isString() || !Convert(cx, query, &text)) return false;
  if (ParseFormQuery(text, entries)) return true;
  JS_ReportOutOfMemory(cx);
  return false;
}

bool ParamsOwner(JSContext* cx, JS::HandleObject params, JS::MutableHandleObject owner) {
  JS::RootedValue slot(cx, JS::GetReservedSlot(params, kParamsOwnerSlot));
  if (slot.isObject()) owner.set(&slot.toObject());
  else owner.set(nullptr);
  return true;
}

// Re-serializes the parameter list and, when the object came from a URL, writes
// the new search back so url.search and url.searchParams stay one value.
bool StoreEntries(State& state, JS::HandleObject params,
                  const std::vector<std::pair<std::string, std::string>>& entries) {
  JSContext* cx = state.cx;
  std::string query = SerializeFormQuery(entries);
  JS::RootedValue text(cx);
  if (!Value(cx, query, &text)) return false;
  JS::SetReservedSlot(params, kParamsQuerySlot, text);
  JS::RootedObject owner(cx);
  if (!ParamsOwner(cx, params, &owner) || !owner) return true;
  JS::RootedValue href(cx, JS::GetReservedSlot(owner, kUrlHrefSlot));
  std::string text_href;
  if (!href.isString() || !Convert(cx, href, &text_href)) return false;
  ruyijs::domain::browser_environment::UrlValue url(text_href);
  if (!url.parsed()) return true;
  url.set_search(query.empty() ? "" : "?" + query);
  JS::RootedValue updated(cx);
  if (!Value(cx, url.href(), &updated)) return false;
  JS::SetReservedSlot(owner, kUrlHrefSlot, updated);
  return true;
}
}  // namespace ruyijs::web_platform
