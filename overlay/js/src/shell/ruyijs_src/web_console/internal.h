#ifndef RUYIJS_WEB_CONSOLE_INTERNAL_H
#define RUYIJS_WEB_CONSOLE_INTERNAL_H
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "web_console/realm.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::web_console {
// One realm's console state: the Context's shared log, this realm's id, and the
// registries the donor keeps on its Console object (counters, timers and the
// group stack). Indentation is derived from that stack at call time.
struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, Log* shared);
  JSContext* cx;
  JS::PersistentRootedObject page;
  JS::PersistentRootedObject console;
  PageHelpers* helpers = nullptr;
  Log* log = nullptr;
  std::unique_ptr<Log> owned;
  uint32_t realm = 0;
  using ClockPort = double (*)(void* context);
  ClockPort clock = nullptr;
  void* clockContext = nullptr;
  std::map<std::string, int> counters;
  std::map<std::string, double> timers;
  std::vector<std::string> groups;
  double Now() const { return clock ? clock(clockContext) : 0.0; }
  static constexpr size_t kLabelLimit = 512;
  static constexpr size_t kCounterLimit = 10000;
  static constexpr size_t kTimerLimit = 10000;
};

// Every content-visible `console` member (donor dom/webidl/Console.webidl) in
// WebIDL order. `length` is 0 for all of them: each takes only optional or
// variadic arguments, which WebIDL does not count.
size_t MethodCount();
const char* MethodName(size_t index);
JSObject* Method(State& state, uint32_t index);
}
#endif
