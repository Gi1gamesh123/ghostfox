#ifndef RUYIJS_WEB_WORKERS_PROCESS_H
#define RUYIJS_WEB_WORKERS_PROCESS_H
#include "runtime/pipe.h"
#include "web_workers/kind.h"
#include <string_view>

namespace ruyijs::web_workers {
enum class StartError { None, Active, InvalidInput, Random, Pipe, Job, Create, Assign, Resume, Allocation };
struct StartResult {
  StartError error = StartError::None;
  unsigned system = 0;
  explicit operator bool() const { return error == StartError::None; }
};
// OS resources only. Worker identities, script URLs, realm state and event
// delivery belong to their owners; this adapter never interprets a frame.
class Process final {
 public:
  Process() = default;
  ~Process();
  Process(const Process&) = delete;
  Process& operator=(const Process&) = delete;
  // `kind` selects the Worker global kind via --ruyijs-worker-kind (dedicated
  // is the default; service/shared add the matching option).
  StartResult Start(std::wstring_view executable, std::string initial,
                    WorkerKind kind = WorkerKind::Dedicated) noexcept;
  bool Queue(std::string frame);
  PipeResult Poll(std::string* frame, bool* received);
  bool Stop();
  bool Running() const;
  unsigned id() const;
  size_t queuedBytes() const { return pipe_.queuedBytes(); }
 private:
  Pipe pipe_;
  void* process_ = nullptr;
  void* job_ = nullptr;
};
}
#endif
