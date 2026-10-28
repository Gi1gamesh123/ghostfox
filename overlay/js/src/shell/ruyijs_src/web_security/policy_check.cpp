// CSP script decisions (docs/owners/CONTENT_SECURITY.md), after the donor's
// nsCSPPolicy::matchingOrDefaultDirective, nsCSPDirective::permits/allows and
// the host/scheme/port source matching in dom/security/nsCSPUtils.cpp.
#include "web_security/policy.h"

#include "domain/browser_environment/url_value.h"

namespace ruyijs::web_security {
namespace {
using ruyijs::domain::browser_environment::UrlValue;
const char* Name(Directive d) {
  switch (d) {
    case Directive::ScriptElem: return "script-src-elem";
    case Directive::ScriptAttr: return "script-src-attr";
    case Directive::Script: return "script-src";
    default: return "default-src";
  }
}
// The directive governing `wanted`, falling back script-src-elem/attr ->
// script-src -> default-src; null when the policy does not restrict it.
const DirectiveEntry* Governing(const Policy& policy, Directive wanted) {
  const Directive chain[3] = {wanted, Directive::Script, Directive::Default};
  const int start = wanted == Directive::Script ? 1 : 0;
  for (int i = start; i < 3; ++i)
    for (const auto& entry : policy.directives)
      if (entry.kind == chain[i]) return &entry;
  return nullptr;
}
bool Has(const DirectiveEntry& entry, Keyword keyword) {
  for (const auto& s : entry.sources)
    if (s.kind == Source::Kind::Keyword && s.keyword == keyword) return true;
  return false;
}
bool HasNonceOrHash(const DirectiveEntry& entry) {
  for (const auto& s : entry.sources)
    if (s.kind == Source::Kind::Nonce || s.kind == Source::Kind::Hash) return true;
  return false;
}
bool NonceMatches(const DirectiveEntry& entry, const std::string& nonce) {
  if (nonce.empty()) return false;
  for (const auto& s : entry.sources)
    if (s.kind == Source::Kind::Nonce && s.value == nonce) return true;
  return false;
}
std::string Normalized(std::string hash) {
  for (char& c : hash) c = c == '-' ? '+' : c == '_' ? '/' : c;
  return hash;
}
bool HashMatches(const DirectiveEntry& entry, const ScriptRequest& request) {
  for (const auto& s : entry.sources) {
    if (s.kind != Source::Kind::Hash) continue;
    const int index = s.algo == "sha256" ? 0 : s.algo == "sha384" ? 1 : 2;
    if (!request.digests[index].empty() && Normalized(s.value) == request.digests[index]) return true;
  }
  return false;
}
std::string DefaultPort(const std::string& scheme) {
  if (scheme == "http" || scheme == "ws") return "80";
  if (scheme == "https" || scheme == "wss") return "443";
  return "";
}
bool SchemePermits(const std::string& enforced, const std::string& scheme, bool self) {
  if (enforced.empty() || enforced == scheme) return true;
  if (enforced == "http" && (scheme == "https" || (self && (scheme == "ws" || scheme == "wss"))))
    return true;
  if (enforced == "https" && scheme == "wss" && self) return true;
  return enforced == "ws" && scheme == "wss";
}
bool HostPermits(const Source& s, const UrlValue& url, const std::string& scheme) {
  if (s.self && s.value.empty()) return false;  // 'self' of an opaque origin matches nothing
  if (s.value == "*" && s.scheme.empty()) return scheme != "blob" && scheme != "data" && scheme != "filesystem";
  if (!SchemePermits(s.scheme, scheme, s.self)) return false;
  std::string host = url.hostname();
  for (char& c : host) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  if (s.value == "*") {
    if (scheme == "blob" || scheme == "data" || scheme == "filesystem") return false;
  } else if (s.value[0] == '*') {
    const std::string suffix = s.value.substr(1);
    if (host.size() < suffix.size() || host.compare(host.size() - suffix.size(), suffix.size(), suffix))
      return false;
  } else if (s.value != host) {
    return false;
  }
  if (s.port != "*") {
    const std::string port = url.port().empty() ? DefaultPort(scheme) : url.port();
    const std::string enforced = s.port.empty() ? DefaultPort(s.scheme) : s.port;
    if (port != enforced && !(s.port.empty() && url.port().empty())) return false;
  }
  if (!s.path.empty()) {
    const std::string& path = url.pathname();
    if (s.path.back() == '/' ? path.rfind(s.path, 0) != 0 : path != s.path) return false;
  }
  return true;
}
bool UrlPermitted(const DirectiveEntry& entry, const std::string& href) {
  const UrlValue url(href);
  std::string scheme = url.protocol();
  if (!scheme.empty()) scheme.pop_back();
  for (auto& c : scheme) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  for (const auto& s : entry.sources) {
    if (s.kind == Source::Kind::Scheme && SchemePermits(s.value, scheme, false)) return true;
    if (s.kind == Source::Kind::Host && HostPermits(s, url, scheme)) return true;
  }
  return false;
}
bool Allows(const DirectiveEntry& entry, const ScriptRequest& request) {
  if (request.eval) return Has(entry, Keyword::UnsafeEval);
  if (NonceMatches(entry, request.nonce) && request.directive != Directive::ScriptAttr) return true;
  if (request.inline_code) {
    if (HashMatches(entry, request) &&
        (request.directive != Directive::ScriptAttr || Has(entry, Keyword::UnsafeHashes)))
      return true;
    // Firefox nsCSPContext::GetAllowsInline also trusts a non-parser-created
    // inline script under strict-dynamic (donor bug 1844290).
    if (Has(entry, Keyword::StrictDynamic) && !request.parser_inserted) return true;
    // allowsAllInlineBehavior: no nonce/hash, and no 'strict-dynamic' for scripts.
    return Has(entry, Keyword::UnsafeInline) && !HasNonceOrHash(entry) &&
           !Has(entry, Keyword::StrictDynamic);
  }
  if (Has(entry, Keyword::StrictDynamic)) return !request.parser_inserted;
  return UrlPermitted(entry, request.url);
}
}  // namespace

std::vector<Violation> Check(const std::vector<Policy>& policies, const ScriptRequest& request,
                             const std::string&) {
  std::vector<Violation> result;
  for (const auto& policy : policies) {
    const Directive wanted = request.eval ? Directive::Script : request.directive;
    const DirectiveEntry* entry = Governing(policy, wanted);
    if (!entry || Allows(*entry, request)) continue;
    // The donor reports the effective directive name (script-src-elem/attr for
    // element and attribute checks, script-src for eval), not the fallback.
    result.push_back({Has(*entry, Keyword::ReportSample), Name(wanted), policy.serialized});
  }
  return result;
}

bool NeedsDigests(const std::vector<Policy>& policies, Directive directive) {
  for (const auto& policy : policies)
    if (const DirectiveEntry* entry = Governing(policy, directive))
      for (const auto& s : entry->sources)
        if (s.kind == Source::Kind::Hash) return true;
  return false;
}
}  // namespace ruyijs::web_security
