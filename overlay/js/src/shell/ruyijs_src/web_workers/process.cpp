#include "web_workers/process.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace ruyijs::web_workers {
Process::~Process() { Stop(); }
bool Process::Running() const {
  return process_ && WaitForSingleObject(process_, 0) == WAIT_TIMEOUT;
}
unsigned Process::id() const { return process_ ? GetProcessId(process_) : 0; }
bool Process::Queue(std::string frame) {
  return Running() && pipe_.Queue(std::move(frame));
}
PipeResult Process::Poll(std::string* frame, bool* received) {
  const auto result = pipe_.Poll(frame, received);
  // Drain already-completed messages before noticing an exited child. A child
  // can exit before connecting, in which case accept would remain pending.
  if (!*received && result == PipeResult::Pending && !pipe_.connected() && !Running())
    return PipeResult::Closed;
  return result;
}
bool Process::Stop() {
  // Closing the Job also covers descendants when its immediate child already
  // exited. The direct termination covers a failed Job assignment at startup.
  if (job_) { CloseHandle(job_);job_ = nullptr; }
  bool stopped = true;
  if (process_) {
    if (Running()) TerminateProcess(process_, 1);
    stopped = WaitForSingleObject(process_, 5000) == WAIT_OBJECT_0;
    CloseHandle(process_);process_ = nullptr;
  }
  pipe_.Close();
  return stopped;
}
}
