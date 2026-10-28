// CSP parsing (docs/owners/CONTENT_SECURITY.md), after the donor's
// dom/security/nsCSPParser.cpp: tokens split on ASCII whitespace, directives on
// ';', policies on ','; the first occurrence of a directive wins; an empty or
// unparsable source list is 'none'. Serialization follows the donor toString,
// which is what SecurityPolicyViolationEvent.originalPolicy reports.
#include "web_security/policy.h"
#include "web_security/policy_names.h"

#include "domain/browser_environment/url_value.h"

namespace ruyijs::web_security {
namespace {
using ruyijs::domain::browser_environment::UrlValue;
bool Space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r'; }
std::string Lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return out;
}
bool Base64Value(std::string_view s) {
  if (s.empty()) return false;
  size_t i = 0;
  for (; i < s.size(); ++i) {
    const char c = s[i];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
          c == '+' || c == '/' || c == '-' || c == '_')) break;
  }
  size_t pad = 0;
  for (; i < s.size() && s[i] == '=' && pad < 2; ++i) ++pad;
  return i == s.size();
}
Directive Kind(const std::string& name) {
  if (name == "default-src") return Directive::Default;
  if (name == "script-src") return Directive::Script;
  if (name == "script-src-elem") return Directive::ScriptElem;
  if (name == "script-src-attr") return Directive::ScriptAttr;
  return Directive::Other;
}
bool KeywordSource(const std::string& lower, Source* out) {
  static const struct { const char* text; Keyword keyword; } kWords[] = {
      {"'none'", Keyword::None}, {"'unsafe-inline'", Keyword::UnsafeInline},
      {"'unsafe-eval'", Keyword::UnsafeEval}, {"'unsafe-hashes'", Keyword::UnsafeHashes},
      {"'strict-dynamic'", Keyword::StrictDynamic}, {"'wasm-unsafe-eval'", Keyword::WasmUnsafeEval},
      {"'report-sample'", Keyword::ReportSample}};
  for (const auto& word : kWords) {
    if (lower != word.text) continue;
    out->kind = Source::Kind::Keyword;
    out->keyword = word.keyword;
    out->text = word.text;
    return true;
  }
  return false;
}
// "'nonce-X'" / "'sha256-X'": quoted, dash, valid base64 value.
bool QuotedSource(const std::string& token, const std::string& lower, Source* out) {
  if (token.size() < 3 || token.front() != '\'' || token.back() != '\'') return false;
  const std::string expr = token.substr(1, token.size() - 2);
  const size_t dash = expr.find('-');
  if (dash == std::string::npos || !Base64Value(std::string_view(expr).substr(dash + 1))) return false;
  const std::string algo = Lower(std::string_view(expr).substr(0, dash));
  if (lower.rfind("'nonce-", 0) == 0) {
    out->kind = Source::Kind::Nonce;
  } else if (algo == "sha256" || algo == "sha384" || algo == "sha512") {
    out->kind = Source::Kind::Hash;
    out->algo = algo;
  } else {
    return false;
  }
  out->value = expr.substr(dash + 1);
  out->text = token;
  return true;
}
bool SchemeChars(std::string_view s) {
  if (s.empty() || !((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z'))) return false;
  for (char c : s)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
          c == '+' || c == '-' || c == '.')) return false;
  return true;
}
// scheme-source "data:" or host-source "[scheme://]host[:port][/path]".
bool LocationSource(const std::string& token, const UrlValue& self, Source* out) {
  std::string rest = token, scheme;
  const size_t colon = token.find(':');
  if (colon != std::string::npos && SchemeChars(std::string_view(token).substr(0, colon))) {
    if (colon + 1 == token.size()) {
      out->kind = Source::Kind::Scheme;
      out->value = Lower(std::string_view(token).substr(0, colon));
      out->text = out->value + ":";
      return true;
    }
    if (token.compare(colon + 1, 2, "//") != 0) {
      if (token.find("//") != std::string::npos) return false;
    } else {
      scheme = Lower(std::string_view(token).substr(0, colon));
      rest = token.substr(colon + 3);
    }
  }
  if (token == "*") { out->kind = Source::Kind::Host; out->value = out->text = "*"; return true; }
  const size_t slash = rest.find('/');
  std::string authority = rest.substr(0, slash);
  std::string path = slash == std::string::npos ? "" : rest.substr(slash);
  std::string port;
  const size_t port_colon = authority.rfind(':');
  if (port_colon != std::string::npos) {
    port = authority.substr(port_colon + 1);
    authority.resize(port_colon);
    if (port.empty()) return false;
    if (port != "*")
      for (char c : port) if (c < '0' || c > '9') return false;
  }
  if (authority.empty()) return false;
  if (authority[0] == '*' && (authority.size() < 3 || authority[1] != '.')) return false;
  for (size_t i = authority[0] == '*' ? 1 : 0; i < authority.size(); ++i) {
    const char c = authority[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
          c == '-' || c == '.')) return false;
  }
  out->kind = Source::Kind::Host;
  out->scheme = scheme.empty() ? Lower(std::string_view(self.protocol()).substr(
                                      0, self.protocol().empty() ? 0 : self.protocol().size() - 1))
                               : scheme;
  out->value = Lower(authority);
  out->port = port;
  out->path = path;
  out->text = out->scheme + "://" + out->value + (port.empty() ? "" : ":" + port) + path;
  return true;
}
void ParseOne(std::string_view text, const UrlValue& self, std::vector<Policy>* out) {
  Policy policy;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find(';', start);
    if (end == std::string_view::npos) end = text.size();
    std::vector<std::string> tokens;
    for (size_t i = start; i < end;) {
      while (i < end && Space(text[i])) ++i;
      size_t j = i;
      while (j < end && !Space(text[j])) ++j;
      if (j > i) tokens.emplace_back(text.substr(i, j - i));
      i = j;
    }
    start = end + 1;
    if (tokens.empty()) continue;
    DirectiveEntry entry;
    entry.name = Lower(tokens[0]);
    if (!KnownDirective(entry.name)) continue;
    bool duplicate = false;
    for (const auto& existing : policy.directives) duplicate |= existing.name == entry.name;
    if (duplicate) continue;
    entry.kind = Kind(entry.name);
    bool keywords_only_sample = true;
    for (size_t i = 1; i < tokens.size(); ++i) {
      Source source;
      const std::string lower = Lower(tokens[i]);
      if (lower == "'self'") {
        source.kind = Source::Kind::Host;
        source.scheme = Lower(std::string_view(self.protocol()).substr(
            0, self.protocol().empty() ? 0 : self.protocol().size() - 1));
        source.value = self.hostname().empty() ? std::string() : Lower(self.hostname());
        source.port = self.port();
        source.text = "'self'";
        source.self = true;
      } else if (!KeywordSource(lower, &source) && !QuotedSource(tokens[i], lower, &source) &&
                 !LocationSource(tokens[i], self, &source)) {
        continue;
      }
      if (!(source.kind == Source::Kind::Keyword && source.keyword == Keyword::ReportSample))
        keywords_only_sample = false;
      entry.sources.push_back(std::move(source));
    }
    if (entry.sources.empty() || keywords_only_sample) {  // An empty source list is 'none'.
      Source none;
      none.kind = Source::Kind::Keyword;
      none.text = "'none'";
      entry.sources.insert(entry.sources.begin(), std::move(none));
    }
    policy.directives.push_back(std::move(entry));
  }
  if (policy.directives.empty()) return;
  for (const auto& directive : policy.directives) {
    if (!policy.serialized.empty()) policy.serialized += "; ";
    policy.serialized += directive.name;
    for (const auto& source : directive.sources) policy.serialized += " " + source.text;
  }
  out->push_back(std::move(policy));
}
}  // namespace

void ParsePolicies(std::string_view text, const std::string& self, std::vector<Policy>* out) {
  const UrlValue base(self);
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find(',', start);
    if (end == std::string_view::npos) end = text.size();
    ParseOne(text.substr(start, end - start), base, out);
    start = end + 1;
  }
}
}  // namespace ruyijs::web_security
