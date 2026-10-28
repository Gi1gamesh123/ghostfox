#include "web_workers/child.h"
#include "web_workers/bootstrap.h"
#include "runtime/exception_capture.h"
#include "jsapi.h"
#include "js/CompilationAndEvaluation.h"
#include "js/SourceText.h"

namespace ruyijs::web_workers {
bool Child::EvaluateStartup(std::string_view frame, const ChildStartup& startup) {
  std::string url;
  std::u16string name;
  std::string_view source, environment;
  if (!DecodeStartup(frame, &url, &name, &source, &environment)) return WebKit::Fail(cx_, "Invalid Worker startup frame");
  url_ = url;
  name_ = JS_NewUCStringCopyN(cx_, name.data(), name.size());
  if (!name_) return false;
  if (startup.prepare && !startup.prepare(startup.context, url, environment)) return false;
  // A network script (not a Blob URL) is fetched here, in the Worker; a failed
  // fetch is the creator's plain `error` event, like a script that fails to compile.
  std::string fetched;
  if (url.rfind("blob:", 0) != 0) {
    std::string final;
    bool ok = false;
    if (!scriptPort_ || !scriptPort_(scriptContext_, url, url, &final, &fetched, &ok)) JS_ClearPendingException(cx_);
    if (!ok) {
      closed_ = true;
      return pipe_.Queue(std::string(1, '\x04'));
    }
    if (!final.empty()) url_ = url = final;
    source = fetched;
  }
  JS::CompileOptions options(cx_);
  options.setFileAndLine(url.c_str(), 1);
  JS::SourceText<mozilla::Utf8Unit> text;
  if (!text.init(cx_, source.data(), source.size(), JS::SourceOwnership::Borrowed)) return false;
  JS::RootedScript script(cx_, JS::Compile(cx_, options, text));
  if (!script) {
    JS_ClearPendingException(cx_);
    closed_ = true;
    // A service worker reports evaluation failures apart from load failures ('\x1d').
    return pipe_.Queue(std::string(1, service() ? '\x1d' : '\x04'));
  }
  if (!JS_ExecuteScript(cx_, script)) {
    CaptureOperationFailure(cx_);
    // A service worker whose script throws during evaluation never installs.
    if (service()) { closed_ = true; return pipe_.Queue(std::string(1, '\x1d')); }
  }
  // Service worker: the script ran, the registry may start the install.
  return !service() || Queue(std::string(1, '\x18'));
}
}
