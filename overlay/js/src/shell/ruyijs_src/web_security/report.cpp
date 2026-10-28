// Violation reporting of the Content Security owner (docs/owners/
// CONTENT_SECURITY.md), after the donor's nsCSPContext::GatherSecurityPolicy-
// ViolationEventData and FireViolationEvent: the init dictionary is built here
// and the trusted event comes from Interaction's one runtime event factory.
#include "web_security/state.h"
#include "interaction/realm.h"
#include "document_model/realm.h"
#include "domain/browser_environment/url_value.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "jsfriendapi.h"

namespace ruyijs::web_security {
namespace {
using ruyijs::domain::browser_environment::UrlValue;
// StripURIForReporting: a non-HTTP(S)/WS(S) URL reports its scheme only;
// otherwise the URL without fragment and userinfo.
std::string Strip(const std::string& href) {
  const UrlValue url(href);
  std::string scheme = url.protocol();
  if (!scheme.empty()) scheme.pop_back();
  if (scheme != "http" && scheme != "https" && scheme != "ws" && scheme != "wss") return scheme;
  std::string out = url.protocol() + "//" + url.host() + url.pathname() + url.search();
  return out;
}
bool Put(JSContext* cx, JS::HandleObject init, const char* name, const std::string& value) {
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(value.data(), value.size())));
  return text && JS_DefineProperty(cx, init, name, text, JSPROP_ENUMERATE);
}
bool PutNumber(JSContext* cx, JS::HandleObject init, const char* name, uint32_t value) {
  JS::RootedValue number(cx, JS::NumberValue(value));
  return JS_DefineProperty(cx, init, name, number, JSPROP_ENUMERATE);
}
bool PutSample(JSContext* cx, JS::HandleObject init, JS::HandleString source, bool report) {
  // Donor CSPViolationData::MaybeTruncateSampleWithEllipsis: bound the copy,
  // extend past a low surrogate at the boundary, and append a localized ellipsis.
  char16_t chars[42];
  size_t length = report && source ? JS_GetStringLength(source) : 0;
  size_t count = length < 40 ? length : 40;
  if (length > count) {
    char16_t next;
    if (!JS_GetStringCharAt(cx, source, count, &next)) return false;
    if (next >= 0xdc00 && next <= 0xdfff) ++count;
  }
  for (size_t i = 0; i < count; ++i)
    if (!JS_GetStringCharAt(cx, source, i, &chars[i])) return false;
  if (length > count) chars[count++] = u'\u2026';
  JS::RootedString text(cx, JS_NewUCStringCopyN(cx, chars, count));
  return text && JS_DefineProperty(cx, init, "sample", text, JSPROP_ENUMERATE);
}
}  // namespace

bool ReportViolation(State& state, JS::HandleObject element, const Violation& violation,
                     const std::string& blocked, JS::HandleString script_source) {
  JSContext* cx = state.cx;
  // The script whose call led here (the inserting script, the eval caller).
  JS::AutoFilename filename;
  uint32_t line = 0;
  JS::ColumnNumberOneOrigin column;
  std::string source;
  if (JS::DescribeScriptedCaller(&filename, cx, &line, &column) && filename.get())
    source = Strip(filename.get());
  else
    line = 0;
  JSAutoRealm realm(cx, state.page);
  const std::string document_uri = Strip(state.DocumentUrl());
  const std::string blocked_uri =
      blocked == "inline" || blocked == "eval" ? blocked : Strip(blocked);
  JS::RootedObject init(cx, JS_NewPlainObject(cx));
  if (!init || !JS_DefineProperty(cx, init, "bubbles", JS::TrueHandleValue, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "composed", JS::TrueHandleValue, JSPROP_ENUMERATE) ||
      !Put(cx, init, "documentURI", document_uri) || !Put(cx, init, "referrer", "") ||
      !Put(cx, init, "blockedURI", blocked_uri) ||
      !Put(cx, init, "violatedDirective", violation.directive) ||
      !Put(cx, init, "effectiveDirective", violation.directive) ||
      !Put(cx, init, "originalPolicy", violation.policy) || !Put(cx, init, "sourceFile", source) ||
      !PutSample(cx, init, script_source, violation.report_sample) ||
      !Put(cx, init, "disposition", "enforce") ||
      !PutNumber(cx, init, "statusCode", 0) || !PutNumber(cx, init, "lineNumber", line) ||
      !PutNumber(cx, init, "columnNumber", line ? column.oneOriginValue() : 0))
    return false;
  // Firefox's external-load check has no triggering element. Inline checks
  // retain their element, and the delivery task decides whether it is connected.
  JS::RootedObject trigger(cx, blocked == "inline" ? element.get() : nullptr);
  return QueueReport(state, trigger, init);
}
}  // namespace ruyijs::web_security
