#ifndef RUYIJS_WEB_WORKERS_OPTIONS_H
#define RUYIJS_WEB_WORKERS_OPTIONS_H
#include "js/RootingAPI.h"
#include <string>
namespace ruyijs::web_workers {
// Constructor dictionary conversion; no persistent Worker or request state.
bool ReadWorkerName(JSContext*, JS::HandleValue options, std::u16string* name);
}
#endif
