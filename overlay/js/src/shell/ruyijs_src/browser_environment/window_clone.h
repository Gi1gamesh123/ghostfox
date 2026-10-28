#ifndef RUYIJS_BROWSER_ENVIRONMENT_WINDOW_CLONE_H
#define RUYIJS_BROWSER_ENVIRONMENT_WINDOW_CLONE_H
#include "js/RootingAPI.h"
namespace ruyijs::browser_environment {
struct State;
bool CloneValue(State&, JS::HandleValue, JS::HandleValue, JS::MutableHandleValue,
                JS::HandleObject destination = nullptr);
bool CloneValue(State&, JS::HandleValue, JS::HandleValue, JS::MutableHandleValue,
                JS::HandleObject destination, JS::MutableHandleObject ports);
}
#endif
