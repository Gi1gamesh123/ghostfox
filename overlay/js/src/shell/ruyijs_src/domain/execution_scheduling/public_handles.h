#pragma once
#include <cstdint>

namespace ruyijs::domain::execution_scheduling {
class PublicHandleSequence {
 public:
  template<class InUse> uint32_t Reserve(InUse inUse) const {
    if (!wrapped_ && last_ < INT32_MAX) return last_ + 1;
    uint32_t candidate = Next(last_);
    const uint32_t start = candidate;
    do {
      if (!inUse(candidate)) return candidate;
      candidate = Next(candidate);
    } while (candidate != start);
    return 0;
  }
  void Commit(uint32_t value) { wrapped_ = wrapped_ || value <= last_; last_ = value; }
 private:
  static uint32_t Next(uint32_t value) { return value >= INT32_MAX ? 1 : value + 1; }
  uint32_t last_ = 0;
  bool wrapped_ = false;
};
}
