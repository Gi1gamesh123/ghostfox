#ifndef RUYIJS_INDEXED_DATABASE_REALM_H
#define RUYIJS_INDEXED_DATABASE_REALM_H
#include <memory>
#include "js/RootingAPI.h"
namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::web_indexeddb {
struct State;
class Realm {
 public:
  Realm(JSContext*, JS::HandleObject, PageHelpers&, interaction::Realm&,
        browser_environment::Environment&, Realm* parent);
  ~Realm();
  bool Install();
  // A Worker realm: the same interfaces, with indexedDB on
  // WorkerGlobalScope.prototype (the Worker process keeps its own store).
  bool InstallWorker(JS::HandleObject globalScopePrototype);
  void SetTaskPort(bool (*)(void*, JS::HandleObject), void*);
  // A later top-level document's realm takes over the shared store's task
  // (docs/owners/INDEXED_DB.md, "Top-level succession").
  bool AdoptStore();
 private:
  bool InstallOn(JS::HandleObject holder, const char* iface);
  std::unique_ptr<State> state_;
};
}
#endif
