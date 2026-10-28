#ifndef RUYIJS_RESOURCE_PAGE_COOKIE_RULES_H
#define RUYIJS_RESOURCE_PAGE_COOKIE_RULES_H
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include <string>

namespace ruyijs::resource_network {
// The cookie rules of the CookieStore dictionaries (page_cookie_rules.cpp). The
// messages are the ones Firefox 158 reports and the path/domain rules are the
// domain cookie owners' (domain/resource_network/cookie_scope.h), so the
// interface keeps no second copy of the cookie grammar.
bool CookieRuleName(const std::string& name, const std::string& value, std::string* error);
bool CookieRulePath(const std::string& path, std::string* error);
bool CookieRuleDomain(const std::string& host, const std::string& domain, std::string* error);
// The CookieSameSite enumeration member: lowercase values, "strict" by default.
bool CookieRuleSameSite(JSContext*, JS::HandleObject dict, std::string* value, std::string* error);
// The nullable DOMTimeStamp member, in the seconds the jar stores.
bool CookieRuleExpires(JSContext*, JS::HandleObject dict, bool* present, double* at);
// The "Set-Cookie" text one assignment parses; a past expiry becomes Max-Age=0.
std::string CookieRuleText(const std::string& name, const std::string& value,
                           const std::string& path, const std::string& domain,
                           bool expires, double at, double now, const std::string& same_site);
// The query URL: the document's own URL, or the given member when it resolves to
// the same origin.
bool CookieRuleQueryURL(const std::string& base, const std::string& given, std::string* url,
                        std::string* error);
}
#endif
