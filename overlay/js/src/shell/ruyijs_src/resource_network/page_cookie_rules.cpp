// The CookieStore dictionary rules: the name/value/path/domain checks Firefox
// reports, the CookieSameSite and DOMTimeStamp members, the query URL and the
// "Set-Cookie" text the one jar parses. Everything here reads the domain cookie
// owners (domain/resource_network/cookie_scope.h), so document.cookie and
// cookieStore.write keep one cookie grammar and one jar.
#include "resource_network/page_cookie_rules.h"
#include "resource_network/state.h"
#include "domain/browser_environment/url_value.h"
#include "domain/resource_network/cookie_date.h"
#include "domain/resource_network/cookie_scope.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include <cmath>

namespace ruyijs::resource_network {
namespace {
using UrlValue = ruyijs::domain::browser_environment::UrlValue;
namespace cookies = ruyijs::domain::resource_network;

bool Invalid(std::string* error, std::string message) { *error = std::move(message); return false; }
}  // namespace

bool CookieRuleName(const std::string& name, const std::string& value, std::string* error) {
  if (name.empty() && value.empty())
    return Invalid(error, "Cookie name and value both cannot be empty");
  for (char c : name) {
    if (c == '=') return Invalid(error, "Cookie name cannot contain '='");
    if (static_cast<unsigned char>(c) < 0x20 || c == ';' || c == ',' || c == ' ')
      return Invalid(error, "Cookie name contains invalid chars");
  }
  for (char c : value)
    if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f || c == ';' || c == ',')
      return Invalid(error, "Cookie value contains invalid chars");
  return true;
}

bool CookieRulePath(const std::string& path, std::string* error) {
  return path.empty() || path.front() == '/' || Invalid(error, "Cookie path must start with '/'");
}

bool CookieRuleDomain(const std::string& host, const std::string& given, std::string* error) {
  if (given.empty()) return true;
  std::string domain = cookies::CookieLower(given);
  if (!domain.empty() && domain.front() == '.') domain.erase(0, 1);
  return cookies::CookieDomain(host, domain) ||
         Invalid(error, "Cookie domain must domain-match current host");
}

bool CookieRuleSameSite(JSContext* cx, JS::HandleObject dict, std::string* value,
                        std::string* error) {
  JS::RootedValue member(cx);
  if (!JS_GetProperty(cx, dict, "sameSite", &member)) return false;
  if (member.isUndefined()) { *value = "strict"; return true; }
  if (!EncodeBodyText(cx, member, value)) return false;
  if (*value == "strict" || *value == "lax" || *value == "none") return true;
  return Invalid(error, "CookieStore.set: '" + *value +
      "' (value of 'sameSite' member of CookieInit) is not a valid value for enumeration "
      "CookieSameSite.");
}

bool CookieRuleExpires(JSContext* cx, JS::HandleObject dict, bool* present, double* at) {
  JS::RootedValue member(cx);
  if (!JS_GetProperty(cx, dict, "expires", &member)) return false;
  if (member.isUndefined() || member.isNull()) return true;
  double ms = 0;
  if (!JS::ToNumber(cx, member, &ms)) return false;
  if (std::isfinite(ms)) { *present = true; *at = ms / 1000.0; }
  return true;
}

std::string CookieRuleText(const std::string& name, const std::string& value,
                           const std::string& path, const std::string& domain,
                           bool expires, double at, double now, const std::string& same_site) {
  std::string text = name + "=" + value + "; Path=" + path;
  if (!domain.empty()) text += "; Domain=" + domain;
  if (expires) {
    double age = at > now ? std::ceil(at - now) : 0;
    if (age > 400.0 * 86400.0) age = 400.0 * 86400.0;
    text += "; Max-Age=" + std::to_string(static_cast<long long>(age));
  }
  if (!same_site.empty()) text += "; SameSite=" + same_site;
  return text;
}

bool CookieRuleQueryURL(const std::string& base, const std::string& given, std::string* url,
                        std::string* error) {
  if (given.empty()) { *url = base; return true; }
  UrlValue resolved;
  if (!UrlValue::Resolve(given, base, &resolved) || resolved.origin() != UrlValue(base).origin())
    return Invalid(error, given + " is not a valid URL.");
  *url = resolved.href();
  return true;
}
}  // namespace ruyijs::resource_network
