// Nested dedicated Workers (Worker inside a DedicatedWorkerGlobalScope): the same Web
// Workers owner as a Window's, bound to the worker realm's own scheduling, object
// URLs, environment snapshot and script URL base. A grandchild's service channel
// (object URLs, caches, fetches) is forwarded over this worker's own channel.
#ifndef RUYIJS_RUNTIME_WORKER_NESTED_H
#define RUYIJS_RUNTIME_WORKER_NESTED_H
struct JSContext;
namespace ruyijs::execution_scheduling { class Realm; }
namespace ruyijs::web_platform { class Realm; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::web_workers { class Realm; class Child; }
namespace ruyijs {
struct NestedWorkerPorts {
  JSContext* cx;
  execution_scheduling::Realm& scheduling;
  web_platform::Realm& platform;
  browser_environment::Environment& environment;
  web_workers::Child& child;
};
// Installs `Worker` in the worker realm and binds its ports to `ports`.
bool InstallNestedWorkers(web_workers::Realm& workers, NestedWorkerPorts& ports);
// Clears the ports before the owners they point to are destroyed.
void UnbindNestedWorkers(web_workers::Realm& workers);
}  // namespace ruyijs
#endif
