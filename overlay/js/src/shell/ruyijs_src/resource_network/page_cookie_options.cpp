// CookieStore argument conversion: one USVString name, or the
// CookieStoreGetOptions / CookieInit / CookieStoreDeleteOptions dictionaries of
// dom/webidl/CookieStore.webidl. The overloads, the argument-count texts and the
// session-cookie defaults (path "/", sameSite "strict", no expiry) are the
// binding's; the rules themselves are page_cookie_rules.cpp and the jar's.
#include "resource_network/page_cookie_store.h"
#include "resource_network/page_cookie_rules.h"
#include "resource_network/state.h"
#include "domain/browser_environment/url_value.h"
#include "resource_network/http_context.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::resource_network {
namespace {
using UrlValue = ruyijs::domain::browser_environment::UrlValue;

bool Invalid(std::string* error, std::string message) { *error = std::move(message); return false; }
// One dictionary member as its DOMString; an absent member leaves |out| alone.
bool Member(JSContext* cx, JS::HandleObject dict, const char* name, std::string* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, dict, name, &value)) return false;
  return value.isUndefined() || EncodeBodyText(cx, value, out);
}
// The document URL a write applies to, or the operation's internal failure.
bool Address(State& s, CookieWriteOptions* out, std::string* error) {
  std::string url;
  double now = 0;
  if (!CookiePageAddress(s, &url, &now))
    return Invalid(error, "CookieStore requires the page network");
  out->url = std::move(url);
  out->now = now;
  return true;
}
}  // namespace

// The realm's document URL and cookie clock through the same ports fetch and
// document.cookie read, so every cookieStore operation stamps one clock.
bool CookiePageAddress(State& s, std::string* url, double* now) {
  HttpContext context;
  if (!s.httpContext || !s.httpContext(s.httpEnvironment, &context)) return false;
  *url = std::move(context.source);
  *now = s.cookieClock ? s.cookieClock(s.cookieClockContext) : 0;
  return true;
}

bool CookieQueryArguments(State& s, bool requireName, const JS::CallArgs& args,
                          CookieQueryOptions* out, std::string* error) {
  JSContext* cx = s.cx;
  CookieWriteOptions address;
  if (!Address(s, &address, error)) return false;
  std::string url;
  if (args.length()) {
    if (args[0].isObject()) {
      JS::RootedObject dict(cx, &args[0].toObject());
      if (!Member(cx, dict, "name", &out->name) || !Member(cx, dict, "url", &url)) return false;
    } else if (!EncodeBodyText(cx, args[0], &out->name)) return false;
  }
  if (requireName && out->name.empty() && url.empty())
    return Invalid(error, "CookieStore.get: CookieStoreGetOptions must not be empty");
  out->now = address.now;
  return CookieRuleQueryURL(address.url, url, &out->url, error);
}

bool CookieSetArguments(State& s, const JS::CallArgs& args, CookieWriteOptions* out,
                        std::string* error) {
  JSContext* cx = s.cx;
  if (args.length() == 0)
    return Invalid(error, "CookieStore.set: 0 is not a valid argument count for any overload.");
  if (!Address(s, out, error)) return false;
  if (args.length() >= 2) {
    // set(USVString name, USVString value): a session cookie on the root path.
    std::string name, value;
    if (!EncodeBodyText(cx, args[0], &name) || !EncodeBodyText(cx, args[1], &value)) return false;
    if (!CookieRuleName(name, value, error)) return false;
    out->text = CookieRuleText(name, value, "/", "", false, 0, out->now, "strict");
    return true;
  }
  if (!args[0].isObject())
    return Invalid(error, "CookieStore.set: Argument 1 can't be converted to a dictionary.");
  JS::RootedObject dict(cx, &args[0].toObject());
  std::string name, value, domain, path, same_site;
  bool expires = false;
  double at = 0;
  if (!Member(cx, dict, "name", &name) || !Member(cx, dict, "value", &value) ||
      !Member(cx, dict, "domain", &domain) || !Member(cx, dict, "path", &path) ||
      !CookieRuleSameSite(cx, dict, &same_site, error) ||
      !CookieRuleExpires(cx, dict, &expires, &at)) return false;
  if (path.empty()) path = "/";
  if (!CookieRuleName(name, value, error) || !CookieRulePath(path, error) ||
      !CookieRuleDomain(UrlValue(out->url).hostname(), domain, error)) return false;
  out->text = CookieRuleText(name, value, path, domain, expires, at, out->now, same_site);
  return true;
}

bool CookieDeleteArguments(State& s, const JS::CallArgs& args, CookieWriteOptions* out,
                           std::string* error) {
  JSContext* cx = s.cx;
  if (args.length() == 0)
    return Invalid(error, "CookieStore.delete: 0 is not a valid argument count for any overload.");
  if (!Address(s, out, error)) return false;
  std::string name, domain, path;
  if (args[0].isObject()) {
    JS::RootedObject dict(cx, &args[0].toObject());
    if (!Member(cx, dict, "name", &name) || !Member(cx, dict, "domain", &domain) ||
        !Member(cx, dict, "path", &path)) return false;
  } else if (!EncodeBodyText(cx, args[0], &name)) return false;
  if (path.empty()) path = "/";
  if (!CookieRuleName(name, std::string(), error) || !CookieRulePath(path, error) ||
      !CookieRuleDomain(UrlValue(out->url).hostname(), domain, error)) return false;
  // A deletion is an assignment whose expiry is already past.
  out->text = CookieRuleText(name, "", path, domain, true, 0, out->now, "");
  return true;
}
}  // namespace ruyijs::resource_network
