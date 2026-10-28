#include "context.h"
#include <cstring>
#include <bcrypt.h>

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


ruyijs_context::~ruyijs_context() { Stop(); }

void ruyijs_context::Stop() {
  active = false;
  if (process) {
    if (WaitForSingleObject(process, 0) == WAIT_TIMEOUT) {
      TerminateProcess(process, 1);
      WaitForSingleObject(process, 5000);
    }
    CloseHandle(process);
    process = nullptr;
  }
  if (pipe != INVALID_HANDLE_VALUE) {
    CloseHandle(pipe);
    pipe = INVALID_HANDLE_VALUE;
  }
}

bool ruyijs_context::Connect(const char* shell, const char* mode, std::string* error) {
  // Only the two modes the runtime accepts; the mode is never script-supplied.
  std::wstring wideMode;
  if (!mode || !strcmp(mode, "default")) wideMode.clear();
  else if (!strcmp(mode, "debug")) wideMode = L"debug";
  else { *error = "Context mode must be default or debug"; return false; }
  int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, shell, -1, nullptr, 0);
  if (!count) { *error = "Invalid UTF-8 executable path"; return false; }
  std::wstring executable(count, L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, shell, -1, executable.data(), count);
  executable.pop_back();
  if (executable.find(L'"') != std::wstring::npos) {
    *error = "Executable path contains a quote";
    return false;
  }
  unsigned char random[16];
  if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG)) {
    *error = "Pipe name generation failed";
    return false;
  }
  std::wstring name = L"\\\\.\\pipe\\ruyijs-";
  for (unsigned char byte : random) {
    name += L"0123456789abcdef"[byte >> 4];
    name += L"0123456789abcdef"[byte & 15];
  }
  pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
      FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
      1, 65536, 65536, 0, nullptr);
  if (pipe == INVALID_HANDLE_VALUE) { *error = "Pipe creation failed"; return false; }
  // Same flags Python passes, so one runtime assembly serves both clients.
  std::wstring command = L"\"" + executable + L"\" --ruyijs-rpc " + name;
  if (!wideMode.empty()) command += L" --ruyijs-mode " + wideMode;
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION information{};
  if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &startup, &information)) {
    *error = "Shell process creation failed: " + std::to_string(GetLastError());
    return false;
  }
  process = information.hProcess;
  CloseHandle(information.hThread);
  DWORD ignored = 0;
  if (!Operation(false, nullptr, 0, &ignored, GetTickCount64() + timeout, true)) {
    *error = "Shell pipe connection failed or timed out";
    return false;
  }
  active = true;
  return true;
}
