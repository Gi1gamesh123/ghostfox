#ifndef RUYIJS_BROWSER_ENVIRONMENT_WINDOW_MESSAGE_OPTIONS_H
#define RUYIJS_BROWSER_ENVIRONMENT_WINDOW_MESSAGE_OPTIONS_H
#include "js/CallArgs.h"
#include "js/RootingAPI.h"
#include <string>
namespace ruyijs::browser_environment {
bool MessageOptions(JSContext*, const JS::CallArgs&, std::string*, JS::MutableHandleValue);
}
#endif
