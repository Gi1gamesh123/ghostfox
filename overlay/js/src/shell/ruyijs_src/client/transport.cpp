#include "context.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include "runtime/ghostfox_port.h"
namespace ruyijs { using ::GetTickCount64; }
#else
#include "runtime/ghostfox_port.h"
namespace ruyijs {
inline Ticks GetTickCount64() { return TicksNow(); }
using ULONGLONG = unsigned long long;
}
#include <fcntl.h>
#include <unistd.h>
#endif


bool ruyijs_context::Operation(bool writing, void* data, DWORD length,
                               DWORD* count, ULONGLONG deadline, bool connecting) {
  OVERLAPPED operation{};
  operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!operation.hEvent) return false;
  BOOL ok = connecting ? ConnectNamedPipe(pipe, &operation)
            : writing ? WriteFile(pipe, data, length, count, &operation)
                      : ReadFile(pipe, data, length, count, &operation);
  if (!ok) {
    DWORD error = GetLastError();
    if (connecting && error == ERROR_PIPE_CONNECTED) {
      ok = TRUE;
    } else if (error == ERROR_IO_PENDING) {
      ULONGLONG now = GetTickCount64();
      DWORD remaining = now >= deadline ? 0 :
        static_cast<DWORD>(std::min<ULONGLONG>(deadline - now, MAXDWORD - 1));
      if (WaitForSingleObject(operation.hEvent, remaining) == WAIT_OBJECT_0) {
        ok = GetOverlappedResult(pipe, &operation, count, FALSE);
      } else {
        CancelIoEx(pipe, &operation);
        GetOverlappedResult(pipe, &operation, count, TRUE);
        ok = FALSE;
      }
    }
  }
  CloseHandle(operation.hEvent);
  return ok != FALSE;
}

bool ruyijs_context::Transfer(bool writing, void* data, uint32_t length,
                              ULONGLONG deadline) {
  auto* bytes = static_cast<char*>(data);
  while (length) {
    if (GetTickCount64() >= deadline) return false;
    DWORD count = 0;
    if (!Operation(writing, bytes, std::min<uint32_t>(length, 65536), &count,
                   deadline) || count == 0) return false;
    bytes += count;
    length -= count;
  }
  return true;
}

bool ruyijs_context::Exchange(const char* data, uint32_t length,
                              std::string* response) {
  const ULONGLONG deadline = GetTickCount64() + timeout;
  if (!WriteFrame(data, length, deadline)) return false;
  while (ReadFrame(response, deadline)) {
    constexpr char marker[] = "{\"event\":\"callback\",";
    if (response->compare(0, sizeof(marker) - 1, marker) != 0) return true;
    if (!callback) return false;
    char* returned = nullptr;
    uint32_t returned_length = 0;
    callback_active = true;
    int status;
    try {
      status = callback(response->data(), static_cast<uint32_t>(response->size()),
                        &returned, &returned_length, callback_user);
    } catch (...) {
      callback_active = false;
      free(returned);
      return false;
    }
    callback_active = false;
    bool ok = status == 0 && returned && returned_length &&
              returned_length <= kRuyijsMaxFrame &&
              WriteFrame(returned, returned_length, deadline);
    free(returned);
    if (!ok) return false;
  }
  return false;
}

bool ruyijs_context::WriteFrame(const char* data, uint32_t length,
                                ULONGLONG deadline) {
  if (!Transfer(true, &length, sizeof(length), deadline) ||
      !Transfer(true, const_cast<char*>(data), length, deadline)) return false;
  return true;
}

bool ruyijs_context::ReadFrame(std::string* response, ULONGLONG deadline) {
  uint32_t received = 0;
  if (!Transfer(false, &received, sizeof(received), deadline) || received == 0 ||
      received > kRuyijsMaxFrame) return false;
  response->resize(received);
  return Transfer(false, response->data(), received, deadline);
}
