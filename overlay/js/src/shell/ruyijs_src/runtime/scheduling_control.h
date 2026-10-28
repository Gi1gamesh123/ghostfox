#ifndef RUYIJS_SCHEDULING_CONTROL_H
#define RUYIJS_SCHEDULING_CONTROL_H
#include "execution_scheduling/realm.h"

namespace ruyijs {
class EnvironmentControl;
class SchedulingControl final {
 public:
  SchedulingControl(JSContext* cx, execution_scheduling::Realm& owner,
                    EnvironmentControl& environment)
      : cx_(cx), owner_(owner), environment_(environment) {}
  void BeginRequest() { owner_.BeginRequest(); }
  void Freeze() { owner_.Freeze(); }
  double ClockMilliseconds() { return owner_.ClockSeconds() * 1000.0; }
  // Lets `ms` of the selected clock pass with the event loop running, as the
  // time between a user's input steps does (runtime/user_input.cpp).
  void Pass(double ms) {
    if (ms > 0) { owner_.Freeze(); owner_.Advance(ms, 16.667, 100000); }
  }
  bool Request(JS::HandleObject request, JS::MutableHandleValue result);
  bool TimerRealm(JS::HandleObject page, JS::HandleObject request, JS::MutableHandleValue result);
 private:
  JSContext* cx_;
  execution_scheduling::Realm& owner_;
  EnvironmentControl& environment_;
  bool Describe(const execution_scheduling::RunResult& state, JS::MutableHandleValue result);
  bool TimerSelection(JS::HandleObject request, const std::string& action, JS::MutableHandleValue result);
  bool MessageSelection(JS::HandleObject request, const std::string& action, JS::MutableHandleValue result);
};
}
#endif
