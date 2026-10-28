#ifndef RUYIJS_WEB_SECURITY_POLICY_H
#define RUYIJS_WEB_SECURITY_POLICY_H
// Content Security Policy model and script decisions (docs/owners/
// CONTENT_SECURITY.md). Engine-independent: no JSAPI, no exceptions. Grammar
// and matching follow the donor's dom/security/nsCSPParser.cpp and
// nsCSPUtils.cpp for the directives this owner enforces.
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ruyijs::web_security {
enum class Directive : uint8_t { Default, Script, ScriptElem, ScriptAttr, Other };
enum class Keyword : uint8_t { None, UnsafeInline, UnsafeEval, UnsafeHashes, StrictDynamic,
                               WasmUnsafeEval, ReportSample };
struct Source {
  enum class Kind : uint8_t { Keyword, Nonce, Hash, Scheme, Host } kind = Kind::Host;
  Keyword keyword = Keyword::None;
  std::string value;   // nonce or hash digest (base64), scheme without ':', host
  std::string algo;    // hash algorithm, lowercase
  std::string scheme;  // host-source scheme ("" never after parsing)
  std::string port;    // "" (default), "*" or digits
  std::string path;    // "" or the path prefix
  std::string text;    // serialization, as the donor's toString
  bool self = false;   // a host source made from 'self' (matches the document origin)
};
struct DirectiveEntry {
  Directive kind = Directive::Other;
  std::string name;  // lowercased
  std::vector<Source> sources;
};
struct Policy {
  std::vector<DirectiveEntry> directives;
  std::string serialized;  // donor nsCSPPolicy::toString: "name src src; name src"
};
// Parses one serialized policy (directive list); `self` is the document URL
// that 'self' and scheme-less host sources take their scheme/origin from.
// Commas separate further policies in one meta value; each is appended.
void ParsePolicies(std::string_view text, const std::string& self, std::vector<Policy>* out);

// What one script-execution check concerns.
struct ScriptRequest {
  Directive directive = Directive::ScriptElem;  // ScriptElem, ScriptAttr or Script (eval)
  bool inline_code = false;     // inline script/handler (no URL)
  bool eval = false;            // eval/Function/string timer
  bool parser_inserted = true;  // 'strict-dynamic' trusts non-parser-inserted scripts
  std::string url;              // external script URL
  std::string nonce;            // element nonce ("" when none)
  std::string digests[3];       // base64 sha256/384/512 of the inline source (empty if unneeded)
};
struct Violation {
  bool report_sample = false;
  std::string directive;  // effective directive name, e.g. "script-src-elem"
  std::string policy;     // the violated policy's serialization
};
// Every policy must allow; each violated policy receives its own report.
std::vector<Violation> Check(const std::vector<Policy>& policies, const ScriptRequest& request,
                             const std::string& self);
// Whether any policy has a hash source applicable to `directive`, so the
// caller computes digests only when they can matter.
bool NeedsDigests(const std::vector<Policy>& policies, Directive directive);
}  // namespace ruyijs::web_security
#endif
