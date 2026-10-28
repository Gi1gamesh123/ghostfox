#include "transport/necko_child_internal.h"
#include <algorithm>

namespace ruyijs::transport {
using child_detail::Error;
NeckoChild::~NeckoChild() {
  if (process_ && WaitForSingleObject(process_, 0) == WAIT_TIMEOUT)
    TerminateProcess(process_, ERROR_PROCESS_ABORTED);
  Close();
}
void NeckoChild::Close() noexcept {
  if (thread_) CloseHandle(thread_);
  if (process_) CloseHandle(process_);
  thread_ = process_ = nullptr;
  pid_ = 0;
}
JobStatus NeckoChild::Wait(uint64_t deadline, uint32_t* exit_code) const noexcept {
  if (!process_ || !exit_code) return {ERROR_INVALID_PARAMETER, "ChildWait"};
  for (;;) {
    const uint64_t now = GetTickCount64();
    const auto duration = static_cast<DWORD>(std::min<uint64_t>(
        deadline > now ? deadline - now : 0, static_cast<uint64_t>(INFINITE) - 1));
    const auto result = WaitForSingleObject(process_, duration);
    if (result == WAIT_OBJECT_0) {
      DWORD code = 0;
      if (!GetExitCodeProcess(process_, &code)) return Error("GetExitCodeProcess");
      *exit_code = code;
      return {};
    }
    if (result == WAIT_FAILED) return Error("WaitForChild");
    if (result != WAIT_TIMEOUT) return {ERROR_GEN_FAILURE, "WaitForChild"};
    if (GetTickCount64() >= deadline) return {ERROR_TIMEOUT, "ChildDeadline"};
  }
}
JobStatus NeckoChild::Terminate(uint32_t exit_code) noexcept {
  if (!process_) return {ERROR_INVALID_PARAMETER, "ChildTerminate"};
  const auto state = WaitForSingleObject(process_, 0);
  if (state == WAIT_OBJECT_0) return {};
  if (state == WAIT_FAILED) return Error("WaitBeforeTerminate");
  if (TerminateProcess(process_, exit_code)) return {};
  const auto failure = Error("TerminateProcess");
  return WaitForSingleObject(process_, 0) == WAIT_OBJECT_0 ? JobStatus{} : failure;
}
JobStatus NeckoChild::Release() noexcept {
  if (!process_) return {};
  const auto state = WaitForSingleObject(process_, 0);
  if (state == WAIT_TIMEOUT) return {ERROR_BUSY, "ChildStillActive"};
  if (state != WAIT_OBJECT_0) return Error("WaitBeforeRelease");
  Close();
  return {};
}
JobStatus NeckoChild::CleanupFailedStart(JobStatus failure, uint64_t deadline) noexcept {
  launch_error_ = failure;
  auto status = Terminate(ERROR_PROCESS_ABORTED);
  if (!status) return status;
  uint32_t exit_code = 0;
  status = Wait(deadline, &exit_code);
  if (!status) return status;
  status = Release();
  return status ? failure : status;
}
}
