#include "resource_network/necko_helper.h"

#ifdef _WIN32

namespace ruyijs::resource_network {
bool NeckoHelper::Start(const WorkerString& command, NeckoPipe& input, NeckoPipe& output, const wchar_t* mode) {
  std::lock_guard<std::mutex> guard(lock_);
  job_.Reset(CreateJobObjectW(nullptr, nullptr));
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!job_ || !SetInformationJobObject(job_.Get(), JobObjectExtendedLimitInformation,
      &limits, sizeof(limits))) return false;
  SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
  NeckoHandle errors(CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ,
      &inherit, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!errors) return false;
  HANDLE inherited[] = {input.Child(), output.Child(), errors.Get()};
  SIZE_T size = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
  std::vector<unsigned char> storage(size);
  auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
  if (!InitializeProcThreadAttributeList(attributes, 1, 0, &size)) return false;
  const BOOL restricted = UpdateProcThreadAttribute(attributes, 0,
      PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr);
  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = input.Child();
  startup.StartupInfo.hStdOutput = output.Child();
  startup.StartupInfo.hStdError = errors.Get();
  startup.lpAttributeList = attributes;
  WorkerString full = command;
  if (mode && *mode) { full += L" "; full += mode; }
  PROCESS_INFORMATION process{};
  // The command prefix carries its own quoting and (for the Python
  // fallback) interpreter arguments; CreateProcessW re-parses it.
  WorkerString application = command;
  auto endQuote = application.find(L'"', 1);
  if (application.front() == L'"' && endQuote != WorkerString::npos) {
    application = application.substr(1, endQuote - 1);
  }
  const BOOL created = restricted && CreateProcessW(application.c_str(), full.data(), nullptr,
      nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
      nullptr, nullptr, &startup.StartupInfo, &process);
  DeleteProcThreadAttributeList(attributes);
  if (!created) return false;
  process_.Reset(process.hProcess);
  NeckoHandle thread(process.hThread);
  return AssignProcessToJobObject(job_.Get(), process_.Get()) &&
      ResumeThread(thread.Get()) != DWORD(-1);
}
bool NeckoWorkerCommand(WorkerString* command) {
  wchar_t override_[MAX_PATH + 1];
  const DWORD overrideLength = GetEnvironmentVariableW(
      L"GHOSTFOX_NECKO_WORKER", override_, MAX_PATH);
  if (overrideLength > 0 && overrideLength < MAX_PATH) {
    *command = std::wstring(L"\"") + override_ + L"\"";
    return true;
  }
  wchar_t self[MAX_PATH];
  const DWORD length = GetModuleFileNameW(nullptr, self, MAX_PATH);
  if (!length || length >= MAX_PATH) return false;
  std::wstring python(self, length);
  auto slash = python.find_last_of(L"\\/");
  if (slash == std::wstring::npos || !slash) return false;
  slash = python.find_last_of(L"\\/", slash - 1);
  if (slash == std::wstring::npos) return false;
  *command = L"\"" + python.substr(0, slash) +
      L"\\vendor\\necko-derived\\python\\python.exe\" -I -B -m ruyijs_necko.oneshot";
  return true;
}
}  // namespace ruyijs::resource_network

#else  // POSIX: fork/exec with the transfer ends as the child's stdio.

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace ruyijs::resource_network {

NeckoHelper::~NeckoHelper() {
  std::lock_guard<std::mutex> guard(lock_);
  if (pid_ > 0) {
    if (kill(pid_, 0) == 0) kill(pid_, SIGKILL);
    int status = 0;
    waitpid(pid_, &status, 0);
    pid_ = -1;
  }
}

bool NeckoHelper::Start(const WorkerString& command, NeckoPipe& input, NeckoPipe& output,
                        const wchar_t* mode) {
  std::lock_guard<std::mutex> guard(lock_);
  if (pid_ > 0) return false;
  std::ostringstream full;
  full << command;
  if (mode && *mode) {
    // The mode words arrive as wide literals; every caller uses ASCII.
    for (const wchar_t* p = mode; *p; ++p) full << static_cast<char>(*p);
  }
  const std::string line = full.str();
  const pid_t child = fork();
  if (child < 0) return false;
  if (child == 0) {
    // Child: the transfer ends become stdin/stdout; the parent's ends and
    // every other descriptor die with the exec.
    if (dup2(input.Child(), STDIN_FILENO) < 0) _exit(127);
    if (dup2(output.Child(), STDOUT_FILENO) < 0) _exit(127);
    int openMax = static_cast<int>(sysconf(_SC_OPEN_MAX));
    for (int fd = 3; fd < openMax && fd < 1024; ++fd) close(fd);
    execl("/bin/sh", "sh", "-c", line.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }
  pid_ = child;
  return true;
}

bool NeckoWorkerCommand(WorkerString* command) {
  // The command string owns its quoting and ends with a separator: Start()
  // appends the mode words directly (the Windows Python command ends in '" ').
  const char* override_ = getenv("GHOSTFOX_NECKO_WORKER");
  if (override_ && *override_) {
    *command = std::string(override_) + " ";
    return true;
  }
  char self[4096];
#if defined(__APPLE__)
  // macOS has no /proc: the dyld query answers with the real executable path
  // (and may request a larger buffer through `size`).
  uint32_t size = sizeof(self) - 1;
  if (_NSGetExecutablePath(self, &size) != 0) return false;
  const ssize_t length = strlen(self);
#else
  const ssize_t length = readlink("/proc/self/exe", self, sizeof(self) - 1);
  if (length <= 0) return false;
  self[length] = '\0';
#endif
  std::string path(self);
  auto slash = path.find_last_of('/');
  if (slash == std::string::npos || !slash) return false;
  *command = "exec '" + path.substr(0, slash) + "/ghostfox-worker' ";
  return true;
}
}  // namespace ruyijs::resource_network

#endif
