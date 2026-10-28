#ifndef RUYIJS_RUNTIME_TOP_LEVEL_H
#define RUYIJS_RUNTIME_TOP_LEVEL_H
#include <memory>
#include <vector>
#include "runtime/browsing_context.h"

namespace ruyijs {
// The top-level traversable's documents (docs/owners/TOP_LEVEL.md). Firefox
// gives every cross-document navigation a new Window in a new realm; here each
// one gets a new BrowsingContext built by the same ordered installers as the
// first, and the document it replaces is retired and kept alive, as a removed
// iframe is. Context-scoped state stays in its existing owners and is shared
// from the first document; this class holds no browser state of its own.
class TopLevel final {
 public:
  TopLevel(JSContext* cx, BrowsingContext& first)
      : cx_(cx), active_(&first), page_(cx, first.page) {}
  ~TopLevel();  // Later documents go youngest first.
  TopLevel(const TopLevel&) = delete;
  TopLevel& operator=(const TopLevel&) = delete;
  BrowsingContext& Active() const { return *active_; }
  // The active document's global. Every RPC request re-reads it.
  JS::HandleObject Page() const { return page_; }
  bool IsActive(const document_model::Realm& document) const {
    return active_->document.get() == &document;
  }
  // Runtime ports, bound by the shared coordinator (runtime/runtime.cpp) and
  // cleared before the owners they reach are destroyed. Scheduling and resource
  // ports are the same ones Frames uses for a child realm; the document port
  // binds what a top-level document has beyond an iframe (navigation, history
  // and error events, audio clock, debug monitor); the host port replays host
  // registrations before the new document's scripts run.
  using ContextPort = bool (*)(void* context, BrowsingContext& target);
  using RetirePort = void (*)(void* context, BrowsingContext& target);
  void SetSchedulingPort(ContextPort port, void* context) { scheduling_ = {port, context}; }
  void SetResourcePort(ContextPort port, void* context) { resource_ = {port, context}; }
  void SetDocumentPort(ContextPort port, void* context) { document_ = {port, context}; }
  void SetHostPort(ContextPort port, void* context) { host_ = {port, context}; }
  // Drops a retired document's scheduled work and clears its ports.
  void SetRetirePort(RetirePort port, void* context) { retire_ = port; retireContext_ = context; }
  // Builds the next document's context in a new realm (same compartment) and
  // makes it active; the previous document is retired. `*created` is the new
  // active context on success.
  bool Advance(BrowsingContext** created);
  // Context shutdown: retires a later active document while the owners its
  // ports reach still exist. The first document is unbound by runtime.cpp.
  void Close();
  void UnbindActivation();
  void UnbindResourceCallbacks();
 private:
  struct Port {
    ContextPort port = nullptr;
    void* context = nullptr;
    bool Run(BrowsingContext& target) const { return !port || port(context, target); }
  };
  // Product bound on top-level documents per Context. Like retired iframes,
  // a retired document is kept alive on purpose: this engine has no GC-driven
  // realm reclamation (docs/DEVIATIONS.md).
  static constexpr size_t kLimit = 64;
  JSContext* cx_;
  BrowsingContext* active_;
  JS::PersistentRootedObject page_;
  Port scheduling_, resource_, document_, host_;
  RetirePort retire_ = nullptr;
  void* retireContext_ = nullptr;
  std::vector<std::unique_ptr<BrowsingContext>> later_;  // Creation order.
};
}
#endif
