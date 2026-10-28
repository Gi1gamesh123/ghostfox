#ifndef RUYIJS_WEB_SECURITY_STATE_H
#define RUYIJS_WEB_SECURITY_STATE_H
#include <string>
#include <vector>
#include "jsapi.h"
#include "web_security/policy.h"

namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::document_model { class Realm; }
namespace ruyijs::web_security {
class Realm;
struct State {
  State(JSContext* context, JS::HandleObject global, interaction::Realm& e,
        browser_environment::Environment& env, document_model::Realm& d)
      : cx(context), page(context, global), events(e), environment(env), document(d),
        held(context) {}
  JSContext* cx;
  JS::PersistentRootedObject page;
  interaction::Realm& events;
  browser_environment::Environment& environment;
  document_model::Realm& document;
  std::vector<Policy> policies;  // The main document's policies, in delivery order.
  bool bound = false;
  uint64_t generation = 0;
  // While the parser produces the document, reports wait here and are
  // delivered when parsing ends, before the document's lifecycle events
  // (Firefox runs those report tasks during the load, before `load`).
  bool holding = false;
  JS::PersistentRootedObjectVector held;
  bool (*task)(void*, JS::HandleObject) = nullptr;
  void* taskContext = nullptr;
  // The document URL, for 'self' and the violation's documentURI.
  std::string DocumentUrl();
};
// Reports one violation: a trusted securitypolicyviolation at `element` when
// it is connected, else at the document (donor nsCSPContext::FireViolationEvent).
// `blocked` is "inline", "eval" or the blocked URL.
bool ReportViolation(State& state, JS::HandleObject element, const Violation& violation,
                     const std::string& blocked, JS::HandleString source);
bool QueueReport(State& state, JS::HandleObject element, JS::HandleObject init);
bool FlushHeldReports(State& state);
// Process-wide engine hook (eval_hook.cpp): registers `realm` for its global's
// JS::Realm so the engine's contentSecurityPolicyAllows finds it.
bool RegisterEvalHook(JSContext* cx, JS::HandleObject page, Realm* realm);
void UnregisterEvalHook(Realm* realm);
}  // namespace ruyijs::web_security
#endif
