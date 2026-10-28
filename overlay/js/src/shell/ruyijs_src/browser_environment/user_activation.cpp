#include "browser_environment/state.h"

namespace ruyijs::browser_environment {
void Environment::SetActivationClock(double (*read)(void*), void* context) {
  state_->activationClock = read;
  state_->activationClockContext = context;
}
void Environment::NotifyActivation(double now) { state_->activation.Notify(now); }
bool Environment::HasHistoryActivation() const { return state_->activation.History(); }
void Environment::ConsumeHistoryActivation() { state_->activation.ConsumeHistory(); }
void Environment::ConsumeTransientActivation() { state_->activation.ConsumeTransient(); }
void Environment::ResetActivation() { state_->activation.Reset(); }
}
