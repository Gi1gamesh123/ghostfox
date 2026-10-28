#ifndef RUYIJS_SCHEDULING_MODEL_H
#define RUYIJS_SCHEDULING_MODEL_H
#include <cstddef>
#include <cstdint>

namespace ruyijs::execution_scheduling {
enum class ErrorCode { None, OutOfMemory, Range, InvalidState };
struct Status { ErrorCode code = ErrorCode::None; const char* message = ""; };
enum class Kind { Timeout, Interval, Frame, Message, Idle, WebTask };
struct TaskSnapshot {
  uint64_t id = 0;
  Kind kind = Kind::Timeout;
  double deadline = 0;
  double timestamp = 0;
  bool didTimeout = false;
  double idleUntil = 0;
};
struct Snapshot {
  bool logical = false;
  double now = 0, epoch = 0, next = 0;
  size_t pending = 0;
  bool has_next = false;
};
struct ClockSnapshot { bool logical = false; double now = 0, epoch = 0; };
class Model final {
 public:
  Model() noexcept = default;
  ~Model() noexcept;
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  Status Initialize(double epoch) noexcept;
  Status Configure(bool logical, double epoch) noexcept;
  Snapshot Inspect() const noexcept;
  ClockSnapshot Clock() const noexcept;
  void SetTime(double now) noexcept;
  Status Schedule(Kind kind, int32_t delay, uint64_t* id) noexcept;
  Status ScheduleIdle(bool hasTimeout, uint32_t timeout, uint64_t* id) noexcept;
  Status ScheduleDelayed(double delay,uint64_t* id) noexcept;
  Status ScheduleWebTask(double delay,int priority,bool continuation,uint64_t* id) noexcept;
  void PrioritizeWebTask(uint64_t id,int priority) noexcept;
  void CancelIdle(uint64_t id) noexcept;
  void CancelWebTask(uint64_t id) noexcept;
  void Cancel(uint64_t id, bool frame) noexcept;
  bool Exists(uint64_t id) const noexcept;
  bool FramePending() const noexcept;
  bool TimerInfo(uint64_t id, TaskSnapshot* task) const noexcept;
  bool Claim(TaskSnapshot* task, bool frameOnly = false, uint64_t selected = 0,
             bool selectedMessage = false) noexcept;
  void Complete(uint64_t id) noexcept;
 private:
  struct Impl;
  Impl* impl_ = nullptr;
};
}
#endif
