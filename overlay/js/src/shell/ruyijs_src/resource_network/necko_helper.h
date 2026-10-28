#pragma once
#include "resource_network/necko_pipe.h"
#include <mutex>
#include <string>
#include <vector>
#ifndef _WIN32
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace ruyijs::resource_network {
#ifdef _WIN32
using WorkerString = std::wstring;
#define GFOX_TEXT(lit) L##lit
#else
using WorkerString = std::string;
#define GFOX_TEXT(lit) lit
#endif

// One spawned transfer worker, killed with the owning context: the Windows
// side parks the child in a kill-on-close job; POSIX makes it a child the
// destructor reaps or terminates.
class NeckoHelper {
 public:
#ifdef _WIN32
  ~NeckoHelper() {
    if (process_ && WaitForSingleObject(process_.Get(), 0) != WAIT_OBJECT_0) {
      TerminateProcess(process_.Get(), 1);
      job_.Reset();
      WaitForSingleObject(process_.Get(), 5000);
    }
  }
#else
  ~NeckoHelper();
#endif
  bool Start(const WorkerString& command, NeckoPipe& input, NeckoPipe& output, const wchar_t* mode);
  void Cancel() {
    std::lock_guard<std::mutex> guard(lock_);
#ifdef _WIN32
    if (process_) TerminateProcess(process_.Get(), ERROR_OPERATION_ABORTED);
#else
    if (pid_ > 0) kill(pid_, SIGABRT);
#endif
  }
  bool Finish(ULONGLONG deadline) {
#ifdef _WIN32
    DWORD status = 1;
    return WaitForSingleObject(process_.Get(), NeckoRemaining(deadline)) == WAIT_OBJECT_0 &&
        GetExitCodeProcess(process_.Get(), &status) && status == 0;
#else
    if (pid_ <= 0) return false;
    const DWORD remaining = NeckoRemaining(deadline);
    for (DWORD waited = 0; waited <= remaining; waited += 5) {
      int status = 1;
      const pid_t done = waitpid(pid_, &status, WNOHANG);
      if (done == pid_) return WIFEXITED(status) && WEXITSTATUS(status) == 0;
      if (done < 0 && errno != EINTR) return false;
      struct timespec pause{0, 5 * 1000 * 1000};
      nanosleep(&pause, nullptr);
    }
    return false;
#endif
  }
 private:
  std::mutex lock_;
#ifdef _WIN32
  NeckoHandle process_, job_;
#else
  pid_t pid_ = -1;
#endif
};
bool NeckoWorkerCommand(WorkerString*);
}
