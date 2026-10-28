#ifndef RUYIJS_CONSOLE_CONTROL_H
#define RUYIJS_CONSOLE_CONTROL_H
#include "web_console/realm.h"

namespace ruyijs {
// RPC adapter for the Console owner's one bounded log. It holds no state of its
// own: it reads the Context-level log the main realm owns, so the C ABI and
// Python see exactly the records the page wrote (T40 A40.5).
class ConsoleControl final {
 public:
  ConsoleControl(JSContext* cx, web_console::Log& owner) : cx_(cx), owner_(owner) {}
  // {op:"console", action:"log"|"clear", clear?}
  bool Request(JS::HandleObject request, JS::MutableHandleValue result);

 private:
  JSContext* cx_;
  web_console::Log& owner_;
};
}
#endif
