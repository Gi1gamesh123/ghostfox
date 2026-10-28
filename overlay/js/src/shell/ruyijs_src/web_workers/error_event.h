#ifndef RUYIJS_WEB_WORKERS_ERROR_EVENT_H
#define RUYIJS_WEB_WORKERS_ERROR_EVENT_H
#include "js/RootingAPI.h"
#include "js/ValueArray.h"
namespace ruyijs::web_workers {
// Adapt Worker errors to Interaction's existing event state, slots and factory.
JSObject* ErrorPrototype(JSContext*, JS::HandleObject global);
bool FireError(JSContext*, JS::HandleObject target, JS::HandleObject init,
               JS::HandleObject prototype, bool* allowed = nullptr);
bool ErrorArguments(JSContext*, JS::HandleValue event, JS::RootedValueArray<5>& arguments);
}
#endif
