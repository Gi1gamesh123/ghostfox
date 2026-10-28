#include "execution_scheduling/state.h"
#include <algorithm>
#include <cmath>
#include <thread>
#include "js/Realm.h"
#include "js/RealmOptions.h"

namespace ruyijs::execution_scheduling {
static JS::ReduceMicrosecondTimePrecisionCallback previousClock = nullptr;
static unsigned bindings = 0;
// One clock drives every realm the scheduling surface was installed into:
// the owner's own page and each iframe realm it installed timers/performance
// into. A realm that belongs to no bound owner falls through to the engine.
static bool OwnsRealm(const State& s, JS::Realm* realm) {
  if (realm && realm == JS::GetObjectRealmOrNull(s.page)) return true;
  for (JSObject* installed : s.realms)
    if (realm && realm == JS::GetObjectRealmOrNull(installed)) return true;
  return false;
}
static double CurrentTime(double usec, JS::RTPCallerTypeToken token, JSContext* cx) {
  JS::Realm* realm = JS::GetCurrentRealmOrNull(cx);
  for (State* s = activeState; s; s = s->previous) {
    if (s->cx != cx || !s->clockBound || !OwnsRealm(*s, realm)) continue;
    auto clock = s->model.Clock();
    if (clock.logical) return std::trunc(clock.epoch + clock.now) * 1000;
    break;
  }
  return previousClock ? previousClock(usec, token, cx) : usec;
}
void State::BindClock() {
  JS::SetRealmReduceTimerPrecisionCallerType(JS::GetCurrentRealmOrNull(cx), {1});
  JS::SetRealmReduceTimerPrecisionCallerType(JS::GetObjectRealmOrNull(page), {0});
  if (!bindings++) {
    previousClock = JS::GetReduceMicrosecondTimePrecisionCallback();
    JS::SetReduceMicrosecondTimePrecisionCallback(CurrentTime);
  }
  clockBound = true;
}
void State::UnbindClock() {
  if (!clockBound) return;
  clockBound = false;
  if (!--bindings) {
    JS::SetReduceMicrosecondTimePrecisionCallback(previousClock);
    previousClock = nullptr;
  }
}
double SystemEpoch() {
  return double(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
}
double State::SystemElapsed() const {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}
void State::SyncClock() {
  if (initialized && !model.Clock().logical) model.SetTime(SystemElapsed());
}
void State::WaitUntil(double time, double step) {
  if (model.Clock().logical) { model.SetTime(time); return; }
  SyncClock();
  while (model.Clock().now < time) {
    double duration = std::min({time - model.Clock().now, step, 50.0});
    // A ready external completion ends the wait: it runs as the next task.
    if (ExternalPending()) {
      if (ExternalReady(uint32_t(std::ceil(duration)))) { SyncClock(); return; }
    } else {
      std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(duration));
    }
    SyncClock();
  }
}
}
