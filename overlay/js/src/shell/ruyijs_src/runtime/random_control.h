#ifndef RUYIJS_RANDOM_CONTROL_H
#define RUYIJS_RANDOM_CONTROL_H
#include "js/RootingAPI.h"

namespace ruyijs {
bool ConfigureRandom(JSContext* cx, JS::HandleObject page, JS::HandleObject request,
                     bool frozen, JS::MutableHandleValue result);
}
#endif
