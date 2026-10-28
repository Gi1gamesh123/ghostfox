#include "resource_network/necko_pipe.h"
#include <string>

#ifdef _WIN32

namespace ruyijs::resource_network {
DWORD NeckoRemaining(ULONGLONG deadline) {
  const auto now = GetTickCount64();
  return now >= deadline ? 0 : DWORD(deadline - now);
}
bool NeckoPipe::Open(bool parentWrites) {
  static LONG sequence = 0;
  const auto name = L"\\\\.\\pipe\\ruyijs-necko-" + std::to_wstring(GetCurrentProcessId()) +
      L"-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(InterlockedIncrement(&sequence));
  parent_.Reset(CreateNamedPipeW(name.c_str(),
      (parentWrites ? PIPE_ACCESS_OUTBOUND : PIPE_ACCESS_INBOUND) |
          FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
      1, 65536, 65536, 0, nullptr));
  if (!parent_) return false;
  SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
  child_.Reset(CreateFileW(name.c_str(), parentWrites ? GENERIC_READ : GENERIC_WRITE,
      0, &inherit, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  event_.Reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
  if (!child_ || !event_) return false;
  OVERLAPPED connected{};
  connected.hEvent = event_.Get();
  // The local client was opened first, so connect completes immediately.
  if (ConnectNamedPipe(parent_.Get(), &connected)) return true;
  const DWORD error = GetLastError();
  if (error == ERROR_IO_PENDING) {
    DWORD ignored = 0;
    CancelIoEx(parent_.Get(), &connected);
    GetOverlappedResult(parent_.Get(), &connected, &ignored, TRUE);
  }
  return error == ERROR_PIPE_CONNECTED;
}
bool NeckoPipe::Transfer(void* data, uint32_t size, bool writing, ULONGLONG deadline) {
  auto* bytes = static_cast<char*>(data);
  uint32_t done = 0;
  while (done < size) {
    if (!NeckoRemaining(deadline)) return false;
    OVERLAPPED io{};
    io.hEvent = event_.Get();
    if (!ResetEvent(io.hEvent)) return false;
    DWORD moved = 0;
    const DWORD amount = size - done > 65536 ? 65536 : size - done;
    const BOOL ready = writing ?
        WriteFile(parent_.Get(), bytes + done, amount, &moved, &io) :
        ReadFile(parent_.Get(), bytes + done, amount, &moved, &io);
    if (!ready) {
      if (GetLastError() != ERROR_IO_PENDING) return false;
      if (WaitForSingleObject(io.hEvent, NeckoRemaining(deadline)) != WAIT_OBJECT_0) {
        CancelIoEx(parent_.Get(), &io);
        // Complete cancellation before the OVERLAPPED or caller buffer dies.
        GetOverlappedResult(parent_.Get(), &io, &moved, TRUE);
        return false;
      }
      if (!GetOverlappedResult(parent_.Get(), &io, &moved, FALSE)) return false;
    }
    if (!moved || moved > amount) return false;
    done += moved;
  }
  return true;
}
bool NeckoPipe::Write(const void* data, uint32_t size, ULONGLONG deadline) {
  return Transfer(const_cast<void*>(data), size, true, deadline);
}
bool NeckoPipe::Read(void* data, uint32_t size, ULONGLONG deadline) {
  return Transfer(data, size, false, deadline);
}
}  // namespace ruyijs::resource_network

#else  // POSIX: an anonymous pipe() pair, poll()-bounded blocking transfers.

#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include "runtime/ghostfox_port.h"

namespace ruyijs::resource_network {

DWORD NeckoRemaining(ULONGLONG deadline) {
  const Ticks now = TicksNow();
  return now >= deadline ? 0 : static_cast<DWORD>(deadline - now);
}

namespace {
bool Await(int fd, short events, ULONGLONG deadline) {
  pollfd wanted{fd, events, 0};
  for (;;) {
    const DWORD remaining = NeckoRemaining(deadline);
    if (!remaining) return false;
    const int ready = poll(&wanted, 1, static_cast<int>(remaining));
    if (ready != -1) return ready > 0;
    if (errno != EINTR) return false;
  }
}
}  // namespace

bool NeckoPipe::Open(bool parentWrites) {
  int ends[2] = {-1, -1};
  if (pipe(ends) != 0) return false;
  // The child end travels through fork/exec; the parent keeps the other.
  parent_.Reset(reinterpret_cast<void*>(static_cast<intptr_t>(
      parentWrites ? ends[1] : ends[0])));
  child_.Reset(reinterpret_cast<void*>(static_cast<intptr_t>(
      parentWrites ? ends[0] : ends[1])));
  return parent_ && child_;
}
bool NeckoPipe::Transfer(void* data, uint32_t size, bool writing, ULONGLONG deadline) {
  auto* bytes = static_cast<char*>(data);
  const int fd = parent_.Get();
  uint32_t done = 0;
  while (done < size) {
    if (!NeckoRemaining(deadline)) return false;
    const uint32_t amount = size - done > 65536 ? 65536 : size - done;
    const ssize_t moved = writing
        ? write(fd, bytes + done, amount)
        : read(fd, bytes + done, amount);
    if (moved > 0) {
      done += static_cast<uint32_t>(moved);
      continue;
    }
    if (moved < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
      if (!Await(fd, writing ? POLLOUT : POLLIN, deadline)) return false;
      continue;
    }
    return false;
  }
  return true;
}
bool NeckoPipe::Write(const void* data, uint32_t size, ULONGLONG deadline) {
  return Transfer(const_cast<void*>(data), size, true, deadline);
}
bool NeckoPipe::Read(void* data, uint32_t size, ULONGLONG deadline) {
  return Transfer(data, size, false, deadline);
}
}  // namespace ruyijs::resource_network

#endif
