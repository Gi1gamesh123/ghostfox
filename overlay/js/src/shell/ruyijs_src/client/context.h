#ifndef RUYIJS_CLIENT_CONTEXT_H
#define RUYIJS_CLIENT_CONTEXT_H
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <string>
#include "ruyijs.h"

struct ruyijs_context {
  HANDLE pipe = INVALID_HANDLE_VALUE;
  HANDLE process = nullptr;
  uint32_t timeout = 10000;
  bool active = false;
  std::atomic<bool> callback_active{false};
  ruyijs_callback callback = nullptr;
  void* callback_user = nullptr;
  std::mutex mutex;
  ~ruyijs_context();
  bool Connect(const char* shell, const char* mode, std::string* error);
  bool Exchange(const char* data, uint32_t length, std::string* response);
  bool Operation(bool writing, void* data, DWORD length, DWORD* count,
                 ULONGLONG deadline, bool connecting = false);
  bool Transfer(bool writing, void* data, uint32_t length, ULONGLONG deadline);
  bool WriteFrame(const char* data, uint32_t length, ULONGLONG deadline);
  bool ReadFrame(std::string* data, ULONGLONG deadline);
  void Stop();
};
constexpr uint32_t kRuyijsMaxFrame = 16 * 1024 * 1024;
#endif
