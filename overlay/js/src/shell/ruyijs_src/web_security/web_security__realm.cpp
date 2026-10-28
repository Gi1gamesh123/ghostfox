// The Content Security owner of one browsing context (docs/owners/
// CONTENT_SECURITY.md): the main document's policy list, meta delivery and
// inheritance, and the script-execution checks the existing execution points
// ask. Decisions are the engine-independent policy.cpp/policy_check.cpp.
#include "web_security/realm.h"
#include "web_security/state.h"
#include "web_crypto/digest_backend.h"
#include "browser_environment/environment.h"
#include "document_model/realm.h"
#include "js/CharacterEncoding.h"
#include "js/Realm.h"
#include "js/String.h"

namespace ruyijs::web_security {
namespace {
std::string Utf8(JSContext* cx, JS::HandleString text) {
  JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, text);
  return bytes ? std::string(bytes.get()) : std::string();
}
std::string Base64(const uint8_t* data, size_t size) {
  static const char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0; i < size; i += 3) {
    const uint32_t n = (uint32_t(data[i]) << 16) | (i + 1 < size ? uint32_t(data[i + 1]) << 8 : 0) |
                       (i + 2 < size ? uint32_t(data[i + 2]) : 0);
    out += kTable[(n >> 18) & 63];
    out += kTable[(n >> 12) & 63];
    out += i + 1 < size ? kTable[(n >> 6) & 63] : '=';
    out += i + 2 < size ? kTable[n & 63] : '=';
  }
  return out;
}
// The inline source's base64 sha256/384/512, as the donor's nsCSPHashSrc
// hashes the UTF-8 text.
void Digests(const std::string& text, std::string digests[3]) {
  const web_crypto::HashAlgorithm kinds[3] = {web_crypto::HashAlgorithm::Sha256,
      web_crypto::HashAlgorithm::Sha384, web_crypto::HashAlgorithm::Sha512};
  for (int i = 0; i < 3; ++i) {
    web_crypto::DigestBytes digest;
    if (web_crypto::ComputeDigest(kinds[i], text.data(), text.size(), &digest))
      digests[i] = Base64(digest.data, digest.size);
  }
}
void MetaPolicy(void* context, const std::u16string& text) {
  static_cast<Realm*>(context)->AddPolicy(text);
}
bool HandlerPort(void* context, JS::HandleObject element, JS::HandleString source, bool* allowed) {
  auto* realm = static_cast<Realm*>(context);
  return realm->AllowHandler(element, source, allowed);
}
bool CheckAndReport(State& state, const ScriptRequest& request, JS::HandleObject element,
                    JS::HandleString source, bool* allowed) {
  const auto violations = Check(state.policies, request, state.DocumentUrl());
  *allowed = violations.empty();
  const std::string blocked = request.eval ? "eval" : request.inline_code ? "inline" : request.url;
  for (const auto& violation : violations)
    if (!ReportViolation(state, element, violation, blocked, source)) return false;
  return true;
}
}  // namespace

std::string State::DocumentUrl() {
  JS::RootedValue href(cx);
  if (!environment.URL(&href) || !href.isString()) { JS_ClearPendingException(cx); return ""; }
  JS::RootedString text(cx, href.toString());
  return Utf8(cx, text);
}

Realm::Realm(JSContext* cx, JS::HandleObject page, interaction::Realm& events,
             browser_environment::Environment& environment, document_model::Realm& document)
    : state_(std::make_unique<State>(cx, page, events, environment, document)) {}
Realm::~Realm() {
  UnregisterEvalHook(this);
  Unbind();
}
bool Realm::Install() {
  if (!RegisterEvalHook(state_->cx, state_->page, this)) return false;
  state_->document.SetSecurityPorts(MetaPolicy, HandlerPort, this);
  state_->bound = true;
  return true;
}
void Realm::Unbind() {
  state_->holding = false;
  state_->held.clear();
  state_->task = nullptr;
  state_->taskContext = nullptr;
  if (!state_->bound) return;
  state_->document.SetSecurityPorts(nullptr, nullptr, nullptr);
  state_->bound = false;
}
void Realm::SetTaskPort(TaskPort port, void* context) {
  state_->task = port;
  state_->taskContext = context;
}
void Realm::AddPolicy(const std::u16string& text) {
  State& state = *state_;
  JS::RootedString value(state.cx, JS_NewUCStringCopyN(state.cx, text.data(), text.size()));
  if (!value) { JS_ClearPendingException(state.cx); return; }
  // Leading/trailing HTML whitespace only; an empty value adds nothing
  // (donor CSP_ApplyMetaCSPToDoc). Meta CSP is never report-only.
  ParsePolicies(Utf8(state.cx, value), state.DocumentUrl(), &state.policies);
}
void Realm::InheritFrom(const Realm& creator) { state_->policies = creator.state_->policies; }
void Realm::Reset() { state_->policies.clear(); ++state_->generation; }
bool Realm::Enforcing() const { return !state_->policies.empty(); }
void Realm::BeginParse() { state_->holding = true; }
bool Realm::EndParse() { return FlushHeldReports(*state_); }

bool Realm::AllowScript(JS::HandleObject element, const std::string* url, JS::HandleString code,
                        bool parser_inserted, bool* allowed) {
  *allowed = true;
  State& state = *state_;
  if (state.policies.empty()) return true;
  JSContext* cx = state.cx;
  ScriptRequest request;
  request.directive = Directive::ScriptElem;
  request.parser_inserted = parser_inserted;
  JS::RootedValue nonce(cx);
  if (!state.document.Attribute(element, u"nonce", &nonce)) return false;
  if (nonce.isString()) {
    JS::RootedString text(cx, nonce.toString());
    request.nonce = Utf8(cx, text);
  }
  if (url) {
    request.url = *url;
  } else {
    request.inline_code = true;
    if (code && NeedsDigests(state.policies, Directive::ScriptElem)) Digests(Utf8(cx, code), request.digests);
  }
  return CheckAndReport(state, request, element, code, allowed);
}

bool Realm::AllowHandler(JS::HandleObject element, JS::HandleString source, bool* allowed) {
  *allowed = true;
  State& state = *state_;
  if (state.policies.empty()) return true;
  ScriptRequest request;
  request.directive = Directive::ScriptAttr;
  request.inline_code = true;
  if (NeedsDigests(state.policies, Directive::ScriptAttr)) Digests(Utf8(state.cx, source), request.digests);
  return CheckAndReport(state, request, element, source, allowed);
}

bool Realm::AllowEval(JS::HandleString source, bool* allowed) {
  *allowed = true;
  State& state = *state_;
  if (state.policies.empty()) return true;
  ScriptRequest request;
  request.directive = Directive::Script;
  request.eval = true;
  JS::RootedObject none(state.cx);
  return CheckAndReport(state, request, none, source, allowed);
}
}  // namespace ruyijs::web_security
