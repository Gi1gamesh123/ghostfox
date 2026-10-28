#include "runtime/pipe.h"
#include <cstdint>
#include <cstring>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

namespace ruyijs {
Pipe::~Pipe() { Close(); }

bool Pipe::Read(std::string* frame) {
  uint32_t length = 0;
  if (!Transfer(&length, sizeof(length), false) || !ValidFrameSize(length))
    return false;
  frame->resize(length);
  return Transfer(frame->data(), length, false);
}

bool Pipe::Write(const std::string& frame) {
  if (!ValidFrameSize(frame.size())) return false;
  uint32_t length = static_cast<uint32_t>(frame.size());
  return Transfer(&length, sizeof(length), true) &&
         Transfer(const_cast<char*>(frame.data()), length, true);
}

#ifdef _WIN32
namespace {
// One name grammar for both ends: the owner creates the pipe and the child
// connects with the same name.
bool ValidName(const char* name) {
  constexpr char prefix[] = "\\\\.\\pipe\\ruyijs-";
  if (strncmp(name, prefix, sizeof(prefix) - 1) != 0) return false;
  const char* suffix = name + sizeof(prefix) - 1;
  if (strlen(suffix) < 16 || strlen(suffix) > 64) return false;
  for (const char* p = suffix; *p; ++p) {
    if (!((*p >= 'a' && *p <= 'f') || (*p >= '0' && *p <= '9') || *p == '-'))
      return false;
  }
  return true;
}
}  // namespace

bool Pipe::Connect(const char* name) {
  // "stdio" binds the frame protocol to the process's own stdin/stdout: the
  // platform-portable transport (a parent process spawns the engine with
  // pipes it owns). Same 4-byte little-endian length framing as a pipe.
  if (name && !strcmp(name, "stdio")) {
    if (handle_) return false;
    handle_ = GetStdHandle(STD_INPUT_HANDLE);
    writeHandle_ = GetStdHandle(STD_OUTPUT_HANDLE);
    stdio_ = true;
    return handle_ && handle_ != INVALID_HANDLE_VALUE &&
           writeHandle_ && writeHandle_ != INVALID_HANDLE_VALUE;
  }
  return OpenClient(name, false);
}
bool Pipe::OpenClient(const char* name, bool asynchronous) {
  if (handle_ || !name || !ValidName(name)) return false;
  HANDLE handle = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, asynchronous ? FILE_FLAG_OVERLAPPED : 0, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return false;
  handle_ = handle;
  return true;
}

bool Pipe::Listen(const char* name) { return OpenServer(name, false); }
bool Pipe::OpenServer(const char* name, bool asynchronous) {
  if (handle_ || !name || !ValidName(name)) return false;
  HANDLE handle = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | (asynchronous ? FILE_FLAG_OVERLAPPED : 0),
      PIPE_TYPE_BYTE | PIPE_REJECT_REMOTE_CLIENTS, 1, 65536, 65536, 0, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return false;
  handle_ = handle;
  return true;
}

// Legacy synchronous callers explicitly opt into a blocking accept. Worker
// process construction uses ListenAsync and Poll on the parent's event loop.
bool Pipe::Accept() {
  if (!handle_ || async_) return false;
  if (!ConnectNamedPipe(handle_, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED)
    return false;
  return true;
}

bool Pipe::Transfer(void* data, unsigned length, bool writing) {
  if (!handle_ || async_) return false;
  void* target = (writing && writeHandle_) ? writeHandle_ : handle_;
  auto* bytes = static_cast<char*>(data);
  while (length) {
    DWORD count = 0;
    BOOL ok = writing ? WriteFile(target, bytes, length, &count, nullptr)
                      : ReadFile(handle_, bytes, length, &count, nullptr);
    if (!ok || !count) return false;
    bytes += count;
    length -= count;
  }
  return true;
}
#else
// POSIX carries the stdio transport; named-pipe server/client modes are a
// Windows capability the page mode does not need.
bool Pipe::Connect(const char* name) {
  if (name && !strcmp(name, "stdio")) {
    if (handle_) return false;
    // Descriptors are stored biased by one: fd 0 must not masquerade as the
    // null "not connected" handle.
    handle_ = reinterpret_cast<void*>(static_cast<intptr_t>(STDIN_FILENO + 1));
    writeHandle_ = reinterpret_cast<void*>(static_cast<intptr_t>(STDOUT_FILENO + 1));
    stdio_ = true;
    return true;
  }
  return OpenClient(name, false);
}
bool Pipe::OpenClient(const char*, bool) { return false; }
bool Pipe::Listen(const char*) { return false; }
bool Pipe::OpenServer(const char*, bool) { return false; }
bool Pipe::Accept() { return false; }

bool Pipe::Transfer(void* data, unsigned length, bool writing) {
  if (!handle_ || async_) return false;
  const int fd = static_cast<int>(reinterpret_cast<intptr_t>(
      (writing && writeHandle_) ? writeHandle_ : handle_)) - 1;
  auto* bytes = static_cast<char*>(data);
  while (length) {
    const ssize_t moved = writing ? write(fd, bytes, length) : read(fd, bytes, length);
    if (moved <= 0) {
      if (moved < 0 && errno == EINTR) continue;
      return false;
    }
    bytes += moved;
    length -= static_cast<unsigned>(moved);
  }
  return true;
}
#endif
}  // namespace ruyijs
