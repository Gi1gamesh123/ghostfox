#include "transport/necko_child_internal.h"
#include <new>

namespace ruyijs::transport {
namespace {
struct StartupOwner {
  HANDLE input = INVALID_HANDLE_VALUE, output = INVALID_HANDLE_VALUE;
  HANDLE handles[2]{};
  std::vector<unsigned char> storage;
  LPPROC_THREAD_ATTRIBUTE_LIST attributes = nullptr;
  ~StartupOwner() {
    if (attributes) DeleteProcThreadAttributeList(attributes);
    if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
  }
  JobStatus Prepare(STARTUPINFOEXW* startup) {
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        &security, OPEN_EXISTING, 0, nullptr);
    if (input == INVALID_HANDLE_VALUE) return child_detail::Error("OpenNulInput");
    output = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         &security, OPEN_EXISTING, 0, nullptr);
    if (output == INVALID_HANDLE_VALUE) return child_detail::Error("OpenNulOutput");
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    if (!size) return child_detail::Error("AttributeListSize");
    storage.resize(size);
    auto list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &size))
      return child_detail::Error("InitializeAttributeList");
    attributes = list;
    handles[0] = input; handles[1] = output;
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                    handles, sizeof(handles), nullptr, nullptr))
      return child_detail::Error("HandleAllowlist");
    startup->StartupInfo.cb = sizeof(*startup);
    startup->StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup->StartupInfo.hStdInput = input;
    startup->StartupInfo.hStdOutput = startup->StartupInfo.hStdError = output;
    startup->lpAttributeList = attributes;
    return {};
  }
};
}
JobStatus NeckoChild::Start(NeckoJob& job, const std::wstring& executable,
                            const std::wstring& cwd, const std::vector<std::wstring>& args,
                            uint64_t cleanup_deadline_ms) noexcept {
  if (valid() || !job.valid()) return {ERROR_INVALID_PARAMETER, "ChildStartState"};
  launch_error_ = {};
  try {
    std::wstring command;
    std::vector<wchar_t> environment;
    auto status = child_detail::Prepare(executable, cwd, args, &command, &environment);
    if (!status) return launch_error_ = status;
    STARTUPINFOEXW startup{};
    StartupOwner owner;
    status = owner.Prepare(&startup);
    if (!status) return launch_error_ = status;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
        DETACHED_PROCESS | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        environment.data(), cwd.c_str(), &startup.StartupInfo, &process))
      return launch_error_ = child_detail::Error("CreateProcessW");
    process_ = process.hProcess; thread_ = process.hThread; pid_ = process.dwProcessId;
    status = job.Assign(process_);
    if (!status) return CleanupFailedStart(status, cleanup_deadline_ms);
    if (ResumeThread(thread_) == static_cast<DWORD>(-1))
      return CleanupFailedStart(child_detail::Error("ResumeThread"), cleanup_deadline_ms);
    return {};
  } catch (const std::bad_alloc&) {
    return launch_error_ = JobStatus{ERROR_NOT_ENOUGH_MEMORY, "ChildAllocation"};
  } catch (...) {
    return launch_error_ = JobStatus{ERROR_GEN_FAILURE, "ChildException"};
  }
}
}
