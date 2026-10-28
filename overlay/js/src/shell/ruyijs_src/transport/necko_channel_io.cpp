#include "transport/necko_channel.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>

namespace ruyijs::transport {
JobStatus NeckoChannel::Operation(bool writing, void* data, uint32_t size,
    uint32_t* transferred, uint64_t deadline, bool connecting, bool idle) noexcept {
  if (idle && (writing || connecting || size != 1)) return {ERROR_INVALID_PARAMETER, "IdleOperation"};
  if (WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) return {ERROR_OPERATION_ABORTED, "ChannelStopped"};
  if (!idle && GetTickCount64() >= deadline) return {WAIT_TIMEOUT, "ChannelDeadline"};
  OVERLAPPED operation{};
  operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!operation.hEvent) return {GetLastError(), "CreateIoEvent"};
  DWORD count = 0;
  BOOL ok = connecting ? ConnectNamedPipe(pipe_, &operation) :
      writing ? WriteFile(pipe_, data, size, &count, &operation) :
                ReadFile(pipe_, data, size, &count, &operation);
  DWORD code = ok ? ERROR_SUCCESS : GetLastError();
  if (connecting && code == ERROR_PIPE_CONNECTED) code = ERROR_SUCCESS;
  if (code == ERROR_IO_PENDING) {
    const auto now = GetTickCount64();
    const DWORD timeout = idle ? INFINITE : now >= deadline ? 0 :
        static_cast<DWORD>(std::min<uint64_t>(deadline - now, MAXDWORD - 1));
    HANDLE events[] = {stop_, operation.hEvent};
    const DWORD waited = WaitForMultipleObjects(2, events, FALSE, timeout);
    if (waited == WAIT_OBJECT_0 + 1) {
      code = GetOverlappedResult(pipe_, &operation, &count, FALSE) ? ERROR_SUCCESS : GetLastError();
      if (code == ERROR_IO_INCOMPLETE) {
        CancelIoEx(pipe_, &operation);
        GetOverlappedResult(pipe_, &operation, &count, TRUE);
      }
    } else {
      code = waited == WAIT_OBJECT_0 ? ERROR_OPERATION_ABORTED :
             waited == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError();
      // Do not destroy the OVERLAPPED/event/caller buffer before kernel I/O
      // acknowledges cancellation. Outer process lifetime is the final bound.
      CancelIoEx(pipe_, &operation);
      GetOverlappedResult(pipe_, &operation, &count, TRUE);
    }
  }
  CloseHandle(operation.hEvent);
  if (!code) *transferred = count;
  return {code, connecting ? "ConnectNamedPipe" : writing ? "WriteFile" : "ReadFile"};
}
JobStatus NeckoChannel::Transfer(bool writing, void* data, uint32_t size, uint64_t deadline,
    uint32_t stall_ms, uint64_t* frame_deadline) noexcept {
  if (!ready_ || !data || !size || size > kMaxTransfer ||
      (!stall_ms && (!deadline || deadline == UINT64_MAX))) return {ERROR_INVALID_PARAMETER, "Transfer"};
  auto& busy = writing ? writing_ : reading_;
  bool expected = false;
  if (!busy.compare_exchange_strong(expected, true)) return {ERROR_BUSY, "Transfer"};
  struct Reset { std::atomic<bool>& busy; ~Reset() { busy = false; } } reset{busy};
  auto* bytes = static_cast<char*>(data);
  bool idle = stall_ms != 0;
  while (size) {
    uint32_t count = 0;
    const auto status = Operation(writing, bytes, idle ? 1 : std::min<uint32_t>(size, 65536),
                                  &count, deadline, false, idle);
    if (!status) { Stop(); return status; }
    if (!count) { Stop(); return {ERROR_BROKEN_PIPE, "TransferEOF"}; }
    if (idle) {
      const auto now = GetTickCount64();
      if (now >= UINT64_MAX - stall_ms) { Stop(); return {ERROR_INVALID_DATA, "FrameDeadline"}; }
      deadline = now + stall_ms;
      idle = false;
    }
    bytes += count; size -= count;
  }
  if (frame_deadline) *frame_deadline = deadline;
  return {};
}
JobStatus NeckoChannel::Read(void* data, uint32_t size, uint64_t deadline) noexcept {
  return Transfer(false, data, size, deadline);
}
JobStatus NeckoChannel::ReadStart(void* data, uint32_t size, uint32_t stall_ms,
    uint64_t* frame_deadline) noexcept {
  if (!frame_deadline || !size || size > 52 || !stall_ms || stall_ms > 60000)
    return {ERROR_INVALID_PARAMETER, "ReadStart"};
  return Transfer(false, data, size, 0, stall_ms, frame_deadline);
}
JobStatus NeckoChannel::Write(const void* data, uint32_t size, uint64_t deadline) noexcept {
  return Transfer(true, const_cast<void*>(data), size, deadline);
}
}
