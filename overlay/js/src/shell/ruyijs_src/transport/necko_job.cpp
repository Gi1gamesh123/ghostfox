#include "transport/necko_job.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <limits>

namespace ruyijs::transport {
namespace {
JobStatus Error(const char* operation) noexcept {
  const auto code = GetLastError();
  return {code ? code : ERROR_GEN_FAILURE, operation};
}
JobStatus Invalid(const char* operation) noexcept {
  return {ERROR_INVALID_PARAMETER, operation};
}
JobEventKind Kind(DWORD message) noexcept {
  switch (message) {
    case JOB_OBJECT_MSG_NEW_PROCESS: return JobEventKind::NewProcess;
    case JOB_OBJECT_MSG_EXIT_PROCESS: return JobEventKind::ExitProcess;
    case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS: return JobEventKind::AbnormalExit;
    case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO: return JobEventKind::Empty;
    case JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT: return JobEventKind::ProcessLimit;
    case JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT: return JobEventKind::ProcessMemoryLimit;
    case JOB_OBJECT_MSG_JOB_MEMORY_LIMIT: return JobEventKind::JobMemoryLimit;
    default: return JobEventKind::Other;
  }
}
}
NeckoJob::~NeckoJob() {
  if (job_) CloseHandle(job_);  // KILL_ON_JOB_CLOSE covers the entire child tree.
  if (completion_) CloseHandle(completion_);
}
JobStatus NeckoJob::Create(JobLimits limits) noexcept {
  if (valid() || !limits.processes || limits.processes > 3 ||
      !limits.process_memory || limits.process_memory > limits.job_memory ||
      limits.job_memory > std::numeric_limits<SIZE_T>::max()) return Invalid("limits");
  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  if (!job) return Error("CreateJobObjectW");
  HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
  if (!port) {
    const auto error = Error("CreateIoCompletionPort");
    CloseHandle(job);
    return error;
  }
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
  info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
      JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION | JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
      JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_JOB_MEMORY;
  info.BasicLimitInformation.ActiveProcessLimit = limits.processes;
  info.ProcessMemoryLimit = static_cast<SIZE_T>(limits.process_memory);
  info.JobMemoryLimit = static_cast<SIZE_T>(limits.job_memory);
  if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
    const auto error = Error("SetJobLimits");
    CloseHandle(port); CloseHandle(job);
    return error;
  }
  JOBOBJECT_ASSOCIATE_COMPLETION_PORT association{this, port};
  if (!SetInformationJobObject(job, JobObjectAssociateCompletionPortInformation,
                              &association, sizeof(association))) {
    const auto error = Error("AssociateCompletionPort");
    CloseHandle(port); CloseHandle(job);
    return error;
  }
  job_ = job; completion_ = port;
  return {};
}
JobStatus NeckoJob::Assign(void* process) noexcept {
  if (!valid() || !process || process == INVALID_HANDLE_VALUE) return Invalid("Assign");
  if (!AssignProcessToJobObject(job_, process)) return Error("AssignProcessToJobObject");
  return {};
}
JobStatus NeckoJob::Terminate(uint32_t exit_code) noexcept {
  if (!valid()) return Invalid("Terminate");
  if (!TerminateJobObject(job_, exit_code)) return Error("TerminateJobObject");
  return {};
}
JobStatus NeckoJob::Inspect(JobSnapshot* output) const noexcept {
  if (!valid() || !output) return Invalid("Inspect");
  JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION memory{};
  if (!QueryInformationJobObject(job_, JobObjectBasicAccountingInformation,
                                 &accounting, sizeof(accounting), nullptr))
    return Error("QueryJobAccounting");
  if (!QueryInformationJobObject(job_, JobObjectExtendedLimitInformation,
                                 &memory, sizeof(memory), nullptr)) return Error("QueryJobMemory");
  *output = {accounting.ActiveProcesses, accounting.TotalProcesses,
             accounting.TotalTerminatedProcesses, memory.PeakProcessMemoryUsed,
             memory.PeakJobMemoryUsed};
  return {};
}
JobStatus NeckoJob::ReadLimits(JobLimits* output) const noexcept {
  if (!valid() || !output) return Invalid("ReadLimits");
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
  if (!QueryInformationJobObject(job_, JobObjectExtendedLimitInformation,
                                 &info, sizeof(info), nullptr)) return Error("QueryJobLimits");
  *output = {info.BasicLimitInformation.ActiveProcessLimit,
             info.ProcessMemoryLimit, info.JobMemoryLimit};
  return {};
}
JobStatus NeckoJob::Poll(JobEvent* output, bool* available) noexcept {
  if (!valid() || !output || !available) return Invalid("Poll");
  *available = false;
  DWORD message = 0;
  ULONG_PTR key = 0;
  OVERLAPPED* data = nullptr;
  if (!GetQueuedCompletionStatus(completion_, &message, &key, &data, 0)) {
    const auto code = GetLastError();
    return code == WAIT_TIMEOUT ? JobStatus{} : JobStatus{code, "GetQueuedCompletionStatus"};
  }
  if (key != reinterpret_cast<ULONG_PTR>(this)) return {ERROR_INVALID_DATA, "CompletionKey"};
  *output = {Kind(message), message, reinterpret_cast<uintptr_t>(data)};
  *available = true;
  return {};
}
JobStatus NeckoJob::ProcessIds(uintptr_t* output, uint32_t capacity, uint32_t* count) const noexcept {
  if (!valid() || !output || !count || !capacity || capacity > 64) return Invalid("ProcessIds");
  struct ProcessList { DWORD assigned, listed; ULONG_PTR ids[64]; } list{};
  if (!QueryInformationJobObject(job_, JobObjectBasicProcessIdList, &list,
                                 sizeof(list), nullptr)) return Error("QueryProcessIds");
  if (list.assigned > list.listed || list.listed > capacity) return {ERROR_MORE_DATA, "ProcessIds"};
  for (DWORD i = 0; i < list.listed; ++i) output[i] = list.ids[i];
  *count = list.listed;
  return {};
}
}
