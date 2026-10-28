#include "transport/necko_channel.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <new>

namespace ruyijs::transport {
namespace {
constexpr wchar_t kPrefix[] = L"\\\\.\\pipe\\ruyijs-necko-";
JobStatus Error(const char* operation) noexcept {
  const auto code = GetLastError();
  return {code ? code : ERROR_GEN_FAILURE, operation};
}
bool ValidName(const std::wstring& name) noexcept {
  constexpr size_t prefix = sizeof(kPrefix) / sizeof(wchar_t) - 1;
  if (name.size() != prefix + 32 || name.compare(0, prefix, kPrefix) != 0) return false;
  for (size_t i = prefix; i < name.size(); ++i)
    if (!((name[i] >= L'0' && name[i] <= L'9') || (name[i] >= L'a' && name[i] <= L'f'))) return false;
  return true;
}
}
NeckoChannel::NeckoChannel() noexcept {
  stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!stop_) initialization_error_ = GetLastError();
}
NeckoChannel::~NeckoChannel() {
  Stop();
  if (pipe_) CloseHandle(pipe_);
  if (stop_) CloseHandle(stop_);
}
void NeckoChannel::Stop() noexcept { if (stop_) SetEvent(stop_); }
JobStatus NeckoChannel::CreateServer() noexcept {
  if (initialized_) return {ERROR_INVALID_STATE, "CreateServer"};
  if (!stop_) return {initialization_error_, "CreateChannelStop"};
  if (WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) return {ERROR_OPERATION_ABORTED, "CreateServer"};
  initialized_ = true;
  unsigned char random[16];
  const auto code = BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (code) return {static_cast<uint32_t>(code), "BCryptGenRandom"};
  try {
    std::wstring name(kPrefix);
    for (auto byte : random) {
      name += L"0123456789abcdef"[byte >> 4]; name += L"0123456789abcdef"[byte & 15];
    }
    HANDLE pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
        FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
        1, 65536, 65536, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) return Error("CreateNamedPipeW");
    name_.swap(name); pipe_ = pipe;
    if (WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) return {ERROR_OPERATION_ABORTED, "CreateServer"};
    return {};
  } catch (...) { return {ERROR_NOT_ENOUGH_MEMORY, "ChannelName"}; }
}
JobStatus NeckoChannel::Accept(uint32_t expected_pid, uint64_t deadline) noexcept {
  if (!pipe_ || ready_ || !expected_pid) return {ERROR_INVALID_PARAMETER, "Accept"};
  bool expected = false;
  if (!reading_.compare_exchange_strong(expected, true)) return {ERROR_BUSY, "Accept"};
  uint32_t count = 0;
  auto status = Operation(false, nullptr, 0, &count, deadline, true);
  if (status) {
    ULONG pid = 0;
    if (!GetNamedPipeClientProcessId(pipe_, &pid)) status = Error("ClientProcessId");
    else if (pid != expected_pid) status = {ERROR_ACCESS_DENIED, "ClientProcessId"};
  }
  if (status) ready_ = true; else Stop();
  reading_ = false;
  return status;
}
JobStatus NeckoChannel::Connect(const std::wstring& name, uint32_t expected_pid, uint64_t deadline) noexcept {
  if (initialized_ || !expected_pid || !ValidName(name)) return {ERROR_INVALID_PARAMETER, "Connect"};
  if (!stop_) return {initialization_error_, "CreateChannelStop"};
  initialized_ = true;
  try { name_ = name; } catch (...) { return {ERROR_NOT_ENOUGH_MEMORY, "ChannelName"}; }
  for (;;) {
    if (WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) return {ERROR_OPERATION_ABORTED, "Connect"};
    const auto now = GetTickCount64();
    if (now >= deadline) return {WAIT_TIMEOUT, "Connect"};
    HANDLE pipe = CreateFileW(name_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (pipe != INVALID_HANDLE_VALUE) {
      ULONG pid = 0;
      if (!GetNamedPipeServerProcessId(pipe, &pid)) {
        const auto error = Error("ServerProcessId"); CloseHandle(pipe); return error;
      }
      if (pid != expected_pid) { CloseHandle(pipe); return {ERROR_ACCESS_DENIED, "ServerProcessId"}; }
      if (WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) {
        CloseHandle(pipe); return {ERROR_OPERATION_ABORTED, "Connect"};
      }
      if (GetTickCount64() >= deadline) { CloseHandle(pipe); return {WAIT_TIMEOUT, "Connect"}; }
      pipe_ = pipe; ready_ = true; return {};
    }
    const DWORD code = GetLastError();
    if (code != ERROR_PIPE_BUSY && code != ERROR_FILE_NOT_FOUND) return {code, "CreateFileW"};
    const DWORD slice = static_cast<DWORD>(std::min<uint64_t>(deadline - now, 25));
    if (code == ERROR_PIPE_BUSY) WaitNamedPipeW(name_.c_str(), slice);
    else if (WaitForSingleObject(stop_, slice) == WAIT_OBJECT_0) return {ERROR_OPERATION_ABORTED, "Connect"};
  }
}
}
