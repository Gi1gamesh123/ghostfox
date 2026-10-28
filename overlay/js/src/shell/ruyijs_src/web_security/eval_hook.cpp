// The engine's dynamic-code hook for the Content Security owner (docs/owners/
// CONTENT_SECURITY.md). One JSContext per process: the current
// JSSecurityCallbacks are copied once (keeping Trusted Types' codeForEvalGets
// and the shell's subsumes) and contentSecurityPolicyAllows is set; it finds
// the owner of the evaluating realm and asks its policies. A realm without an
// owner or without policies falls through to the previous checker.
#include "web_security/realm.h"
#include "web_security/state.h"
#include "js/Principals.h"
#include "js/Realm.h"
#include <utility>
#include <vector>

namespace ruyijs::web_security {
namespace {
std::vector<std::pair<JS::Realm*, Realm*>>& Registry() {
  static std::vector<std::pair<JS::Realm*, Realm*>> registry;
  return registry;
}
JSCSPEvalChecker previous = nullptr;
bool Allows(JSContext* cx, JS::RuntimeCode kind, JS::HandleString code, JS::CompilationType type,
            JS::Handle<JS::StackGCVector<JSString*>> parameters, JS::HandleString body,
            JS::Handle<JS::StackGCVector<JS::Value>> parameterArgs, JS::HandleValue bodyArg,
            bool* canCompile) {
  *canCompile = true;
  if (previous && !previous(cx, kind, code, type, parameters, body, parameterArgs, bodyArg, canCompile))
    return false;
  if (!*canCompile) return true;
  // 'wasm-unsafe-eval' is not enforced (D133): WebAssembly compiles as before.
  if (kind != JS::RuntimeCode::JS) return true;
  JS::Realm* current = JS::GetCurrentRealmOrNull(cx);
  for (const auto& [realm, owner] : Registry())
    if (realm == current) return owner->AllowEval(code, canCompile);
  return true;
}
bool Install(JSContext* cx) {
  static JSSecurityCallbacks callbacks;
  static bool installed = false;
  if (installed) return true;
  if (const JSSecurityCallbacks* current = JS_GetSecurityCallbacks(cx)) callbacks = *current;
  previous = callbacks.contentSecurityPolicyAllows;
  callbacks.contentSecurityPolicyAllows = Allows;
  JS_SetSecurityCallbacks(cx, &callbacks);
  installed = true;
  return true;
}
}  // namespace

bool RegisterEvalHook(JSContext* cx, JS::HandleObject page, Realm* realm) {
  if (!Install(cx)) return false;
  JS::Realm* target = JS::GetObjectRealmOrNull(page);
  if (!target) return false;
  for (auto& entry : Registry())
    if (entry.first == target) { entry.second = realm; return true; }
  Registry().emplace_back(target, realm);
  return true;
}
void UnregisterEvalHook(Realm* realm) {
  auto& registry = Registry();
  for (size_t i = 0; i < registry.size();)
    if (registry[i].second == realm) registry.erase(registry.begin() + i); else ++i;
}
}  // namespace ruyijs::web_security
