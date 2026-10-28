// WorkerGlobalScope operations the Worker owner itself defines (docs/owners/
// WORKER.md, 2026-10-21): importScripts (classic scripts fetched one by one
// through the bound script port, then run in this global), dump (content
// dump is disabled by browser.dom.window.dump.enabled, as on Window) and
// reportError (the same ErrorEvent + parent propagation an uncaught error
// takes). OrderGlobalScope re-defines WorkerGlobalScope.prototype's own keys in
// Firefox's binding order once every installer has run.
#include "web_workers/child.h"
#include "document_model/exception.h"
#include "runtime/binding_order.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/CompilationAndEvaluation.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/SourceText.h"
#include <iterator>

namespace ruyijs::web_workers {
namespace {
enum ScopeOp { kImportScripts, kDump, kReportError };
// Firefox 158 WorkerGlobalScope.prototype own keys (live dedicated Worker probe,
// artifacts/worker-parity/firefox-worker.json).
const char* const kScopeOrder[] = {
    "importScripts", "dump", "reportError", "btoa", "atob", "setTimeout", "clearTimeout", "setInterval",
    "clearInterval", "queueMicrotask", "createImageBitmap", "structuredClone", "fetch", "self", "location",
    "navigator", "onerror", "onlanguagechange", "onoffline", "ononline", "onrejectionhandled",
    "onunhandledrejection", "performance", "fonts", "crypto", "origin", "crossOriginIsolated",
    "isSecureContext", "indexedDB", "caches", "scheduler", "trustedTypes"};
}  // namespace

void Child::SetScriptPort(ScriptPort port, void* context) {
  scriptPort_ = port;
  scriptContext_ = context;
}

bool Child::ImportScripts(const JS::CallArgs& args) {
  for (unsigned i = 0; i < args.length(); ++i) {
    JS::RootedString text(cx_, JS::ToString(cx_, args[i]));
    std::string input, url, source;
    if (!text || !ToUTF8(cx_, text, &input)) return false;
    bool ok = false;
    if (scriptPort_ && !scriptPort_(scriptContext_, url_, input, &url, &source, &ok)) {
      if (JS_IsExceptionPending(cx_)) return false;
      ok = false;
    }
    if (!ok)
      return document_model::ThrowException(cx_, exception_, "NetworkError",
                                            ("Failed to load worker script at \"" + input + "\"").c_str());
    JS::CompileOptions options(cx_);
    options.setFileAndLine(url.c_str(), 1);
    JS::SourceText<mozilla::Utf8Unit> script;
    JS::RootedValue ignored(cx_);
    if (!script.init(cx_, source.data(), source.size(), JS::SourceOwnership::Borrowed) ||
        !JS::Evaluate(cx_, options, script, &ignored)) return false;
  }
  args.rval().setUndefined();
  return true;
}

bool Child::ScopeMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto& child = *static_cast<Child*>(WebKit::Owner(args));
  if (!args.thisv().isNullOrUndefined() &&
      (!args.thisv().isObject() || &args.thisv().toObject() != child.kit_.page))
    return WebKit::Fail(cx, "WorkerGlobalScope method receiver is invalid");
  switch (WebKit::Slot(args)) {
    case kImportScripts: return child.ImportScripts(args);
    case kDump:
      if (argc && !JS::ToString(cx, args[0])) return false;
      args.rval().setUndefined();
      return true;
    case kReportError: {
      if (!argc) return WebKit::Fail(cx, "WorkerGlobalScope.reportError: At least 1 argument required, but only 0 passed");
      JS::RootedObject noStack(cx);
      args.rval().setUndefined();
      return child.ReportError(args[0], noStack);
    }
  }
  return WebKit::Fail(cx, "Unknown WorkerGlobalScope method");
}

bool Child::InstallScopeMethods() {
  const struct { const char* name; unsigned length; int op; } methods[] = {
      {"importScripts", 0, kImportScripts}, {"dump", 0, kDump}, {"reportError", 1, kReportError}};
  for (const auto& method : methods)
    if (!kit_.Method(globalScope_, "WorkerGlobalScope", method.name, ScopeMethod, method.length, method.op))
      return false;
  return true;
}

bool Child::OrderGlobalScope() {
  JS::RootedObject prototype(cx_, globalScope_.prototype);
  return prototype && OrderOwnKeys(cx_, prototype, kScopeOrder, std::size(kScopeOrder));
}
}  // namespace ruyijs::web_workers
