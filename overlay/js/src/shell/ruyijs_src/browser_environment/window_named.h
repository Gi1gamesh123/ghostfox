#ifndef RUYIJS_BROWSER_ENVIRONMENT_WINDOW_NAMED_H
#define RUYIJS_BROWSER_ENVIRONMENT_WINDOW_NAMED_H
#include "js/RootingAPI.h"
namespace ruyijs::browser_environment {
struct State;
JSObject* NewWindowNamedProperties(State&, JS::HandleObject prototype);
}
#endif
