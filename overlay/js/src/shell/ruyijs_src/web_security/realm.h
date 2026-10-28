#ifndef RUYIJS_WEB_SECURITY_REALM_H
#define RUYIJS_WEB_SECURITY_REALM_H
// Content Security Policy owner of one browsing context (docs/owners/
// CONTENT_SECURITY.md). It keeps the policy list of the context's main
// document and answers the script-execution checks the existing execution
// points ask; violations are delivered through Interaction's one event
// factory. It installs no constructor, prototype or global property.
#include <memory>
#include <string>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::document_model { class Realm; }
namespace ruyijs::web_security {
struct State;
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, interaction::Realm& events,
        browser_environment::Environment& environment, document_model::Realm& document);
  ~Realm();  // Leaves the engine-hook registry.
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  // Registers this realm for the engine's eval hook (installed process-wide on
  // first use) and binds the Document Model ports. Run once by the shared
  // coordinator after Document Model and Interaction installed.
  bool Install();
  // Unbinds the Document Model and task ports; run before those owners go.
  void Unbind();
  // Violation events are dispatched as tasks (donor CSPReportSenderRunnable);
  // bound by the shared coordinator to the Context's one task queue.
  using TaskPort = bool (*)(void* context, JS::HandleObject task);
  void SetTaskPort(TaskPort port, void* context);
  // Appends the policies of one <meta http-equiv="Content-Security-Policy">.
  void AddPolicy(const std::u16string& text);
  // A srcdoc / initial about:blank document inherits its creator's policies.
  void InheritFrom(const Realm& creator);
  // A document replaced in this realm (host `load`) starts without policies.
  void Reset();
  bool Enforcing() const;
  // The page-load coordinator brackets parsing: reports made meanwhile are
  // delivered at EndParse, before the document's lifecycle events.
  void BeginParse();
  bool EndParse();
  // Classic script element check, made by the one script preparation point.
  // `url` is null for an inline script; `code` is its text. On a block the
  // violation is reported and *allowed is false.
  bool AllowScript(JS::HandleObject element, const std::string* url, JS::HandleString code,
                   bool parser_inserted, bool* allowed);
  // Event handler content attribute, before it compiles.
  bool AllowHandler(JS::HandleObject element, JS::HandleString source, bool* allowed);
  // eval / Function / string timers of this realm.
  bool AllowEval(JS::HandleString source, bool* allowed);
 private:
  std::unique_ptr<State> state_;
};
}  // namespace ruyijs::web_security
#endif
