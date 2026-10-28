#pragma once
#include "transport/necko_job.h"
#include <string>
#include <vector>

namespace ruyijs::transport {
// Single-threaded process owner. The supplied Job must outlive the child tree.
// The supervisor validates package identity and stops/joins all channel users.
class NeckoChild final {
 public:
  NeckoChild() noexcept = default;
  // Last-resort termination only; explicit Wait/Release proves direct-child exit.
  ~NeckoChild();
  NeckoChild(const NeckoChild&) = delete;
  NeckoChild& operator=(const NeckoChild&) = delete;
  JobStatus Start(NeckoJob& job, const std::wstring& executable,
                  const std::wstring& cwd, const std::vector<std::wstring>& args,
                  uint64_t cleanup_deadline_ms) noexcept;
  JobStatus Wait(uint64_t deadline_ms, uint32_t* exit_code) const noexcept;
  JobStatus Terminate(uint32_t exit_code) noexcept;
  // Active processes retain both handles and return ERROR_BUSY.
  JobStatus Release() noexcept;
  bool valid() const noexcept { return process_ != nullptr; }
  uint32_t pid() const noexcept { return pid_; }
  JobStatus launch_error() const noexcept { return launch_error_; }
 private:
  JobStatus CleanupFailedStart(JobStatus failure, uint64_t deadline) noexcept;
  void Close() noexcept;
  void* process_ = nullptr;
  void* thread_ = nullptr;
  uint32_t pid_ = 0;
  JobStatus launch_error_{};
};
}
