#ifndef RUYIJS_TRANSPORT_NECKO_CHANNEL_H
#define RUYIJS_TRANSPORT_NECKO_CHANNEL_H
#include "transport/necko_job.h"
#include <atomic>
#include <cstdint>
#include <string>

namespace ruyijs::transport {
// Initialization is single-threaded; Stop may run during it. After publication,
// one reader and one writer may run concurrently. Join all users before destroy.
class NeckoChannel final {
 public:
  static constexpr uint32_t kMaxTransfer = 262144 + 4;
  NeckoChannel() noexcept;
  ~NeckoChannel();
  NeckoChannel(const NeckoChannel&) = delete;
  NeckoChannel& operator=(const NeckoChannel&) = delete;
  JobStatus CreateServer() noexcept;
  JobStatus Accept(uint32_t expected_pid, uint64_t deadline_ms) noexcept;
  JobStatus Connect(const std::wstring& name, uint32_t expected_pid, uint64_t deadline_ms) noexcept;
  JobStatus Read(void* data, uint32_t size, uint64_t deadline_ms) noexcept;
  JobStatus ReadStart(void* data, uint32_t size, uint32_t stall_ms,
                      uint64_t* frame_deadline) noexcept;
  JobStatus Write(const void* data, uint32_t size, uint64_t deadline_ms) noexcept;
  void Stop() noexcept;
  const std::wstring& name() const noexcept { return name_; }
 private:
  JobStatus Transfer(bool writing, void* data, uint32_t size, uint64_t deadline,
                     uint32_t stall_ms = 0, uint64_t* frame_deadline = nullptr) noexcept;
  JobStatus Operation(bool writing, void* data, uint32_t size, uint32_t* transferred,
                      uint64_t deadline, bool connecting = false, bool idle = false) noexcept;
  void* pipe_ = nullptr;
  void* stop_ = nullptr;
  uint32_t initialization_error_ = 0;
  bool initialized_ = false;
  std::wstring name_;
  std::atomic<bool> reading_{false}, writing_{false}, ready_{false};
};
}
#endif
