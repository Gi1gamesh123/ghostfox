#ifndef RUYIJS_WORKER_TASKS_H
#define RUYIJS_WORKER_TASKS_H
namespace ruyijs::web_workers { class Realm; }
namespace ruyijs::execution_scheduling { class Realm; }
namespace ruyijs {
void BindWorkerTasks(web_workers::Realm&, execution_scheduling::Realm&);
}
#endif
