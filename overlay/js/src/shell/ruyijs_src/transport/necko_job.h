#ifndef RUYIJS_TRANSPORT_NECKO_JOB_H
#define RUYIJS_TRANSPORT_NECKO_JOB_H
#include <cstdint>

namespace ruyijs::transport {
struct JobLimits {
  uint32_t processes = 3;
  uint64_t process_memory = 1536ull * 1024 * 1024;
  uint64_t job_memory = 2048ull * 1024 * 1024;
};
struct JobStatus {
  uint32_t code = 0;
  const char* operation = "";
  explicit operator bool() const noexcept { return code == 0; }
};
struct JobSnapshot {
  uint32_t active = 0, total = 0, terminated = 0;
  uint64_t peak_process_memory = 0, peak_job_memory = 0;
};
enum class JobEventKind {
  Other, NewProcess, ExitProcess, AbnormalExit, Empty,
  ProcessLimit, ProcessMemoryLimit, JobMemoryLimit
};
struct JobEvent {
  JobEventKind kind = JobEventKind::Other;
  uint32_t native_message = 0;
  uintptr_t process_id = 0;
};
// Infrastructure only. The supervisor creates suspended children and resumes
// them only after Assign succeeds. No handle is inherited by those children.
class NeckoJob final {
 public:
  NeckoJob() noexcept = default;
  ~NeckoJob();
  NeckoJob(const NeckoJob&) = delete;
  NeckoJob& operator=(const NeckoJob&) = delete;
  JobStatus Create(JobLimits limits = {}) noexcept;
  JobStatus Assign(void* process) noexcept;
  JobStatus Terminate(uint32_t exit_code) noexcept;
  JobStatus Inspect(JobSnapshot* output) const noexcept;
  JobStatus ReadLimits(JobLimits* output) const noexcept;
  JobStatus ProcessIds(uintptr_t* output, uint32_t capacity, uint32_t* count) const noexcept;
  // Nonblocking. Completion notifications supplement Inspect/process handles;
  // Windows does not guarantee every notification will be delivered.
  JobStatus Poll(JobEvent* output, bool* available) noexcept;
  bool valid() const noexcept { return job_ != nullptr; }
 private:
  void* job_ = nullptr;
  void* completion_ = nullptr;
};
}
#endif
