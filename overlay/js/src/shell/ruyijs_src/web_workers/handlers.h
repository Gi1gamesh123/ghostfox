#ifndef RUYIJS_WEB_WORKERS_HANDLERS_H
#define RUYIJS_WEB_WORKERS_HANDLERS_H
#include "js/RootingAPI.h"
namespace ruyijs { class PageHelpers; }
namespace ruyijs::web_workers {
enum class HandlerExposure { Worker, SharedWorker, GlobalScope, DedicatedScope, SharedScope };
JSObject* NewHandlerStore(JSContext*, JS::HandleObject eventTarget);
bool InstallHandlers(JSContext*, PageHelpers&, JS::HandleObject target,
                     JS::HandleObject globalStore = nullptr, JS::HandleObject global = nullptr,
                     HandlerExposure exposure = HandlerExposure::Worker);
}
#endif
