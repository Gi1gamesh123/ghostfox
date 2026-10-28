#ifndef RUYIJS_WEB_WORKERS_KIND_H
#define RUYIJS_WEB_WORKERS_KIND_H
namespace ruyijs::web_workers {
// The three global kinds a Worker process can run (docs/owners/WORKER.md,
// "Service-kind worker processes" and "Shared workers"). The shell option
// --ruyijs-worker-kind carries the lowercase spelling.
enum class WorkerKind { Dedicated, Service, Shared };
}
#endif
