#ifndef RUYIJS_RUNTIME_USER_INPUT_H
#define RUYIJS_RUNTIME_USER_INPUT_H
#include "js/TypeDecls.h"

namespace ruyijs {
class TopLevel;
class SchedulingControl;
// The `user` RPC (ARCHITECTURE.md, host user input): `click`, `type` and
// `press` as a real user does them in Firefox. This application coordinator
// holds no state; it orders Interaction's trusted events and Document Model's
// focus, editing and activation default actions, and Web Geometry supplies
// the pointer position. Python and the C ABI share it.
bool UserInputRequest(JSContext* cx, TopLevel& top, JS::HandleObject request,
                      SchedulingControl& scheduling, JS::MutableHandleValue result);
}
#endif
