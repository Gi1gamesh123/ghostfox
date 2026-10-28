// The one production installer of the Trusted Types owner for a Window or
// Worker realm, and the process-wide HostGetCodeForEval registration.
#include "web_trusted_types/internal.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Principals.h"
#include "js/Realm.h"

namespace ruyijs::web_trusted_types {
namespace {
// The shell installs its JSSecurityCallbacks before any runtime code runs. They
// are copied once with codeForEvalGets added, keeping its CSP check and
// subsumes hook; one process has one JSContext.
bool InstallEvalHook(JSContext* cx) {
  static JSSecurityCallbacks callbacks;
  static bool installed = false;
  if (installed) return true;
  if (const JSSecurityCallbacks* current = JS_GetSecurityCallbacks(cx)) callbacks = *current;
  callbacks.codeForEvalGets = CodeForEval;
  JS_SetSecurityCallbacks(cx, &callbacks);
  installed = true;
  return true;
}
}
Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers)
    : state_(std::make_unique<State>(cx, page, helpers)) {}
Realm::~Realm() = default;
bool Realm::Install(JS::HandleObject globalPrototype) {
  State& state = *state_;
  JSAutoRealm realm(state.cx, state.kit.page);
  JS::RootedObject target(state.cx, globalPrototype ? globalPrototype.get()
                                                    : state.kit.page.get());
  return InstallEvalHook(state.cx) && InstallValues(state) && InstallPolicy(state) &&
         InstallFactory(state, target, globalPrototype ? "WorkerGlobalScope" : "Window");
}
}
