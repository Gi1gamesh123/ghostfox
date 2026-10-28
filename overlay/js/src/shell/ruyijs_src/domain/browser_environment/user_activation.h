#pragma once

namespace ruyijs::domain::browser_environment {
// Document runtime value; the shared fingerprint Model never owns activation.
// Times come from the original Scheduling owner, in milliseconds. Firefox's
// history marker compares timestamps exactly, independently of transient expiry.
class UserActivation final {
 public:
  void Notify(double now) { sticky_ = transient_ = true; last_ = now; }
  bool Sticky() const { return sticky_; }
  bool Transient(double now, double timeout = 5000.0) const {
    return transient_ && (timeout <= 0 || now - last_ <= timeout);
  }
  bool History() const { return sticky_ && (!historyConsumed_ || history_ != last_); }
  void ConsumeHistory() { history_ = last_; historyConsumed_ = sticky_; }
  void ConsumeTransient() { transient_ = false; }
  void Reset() { *this = UserActivation{}; }
 private:
  double last_ = 0, history_ = 0;
  bool sticky_ = false, transient_ = false, historyConsumed_ = false;
};
}
