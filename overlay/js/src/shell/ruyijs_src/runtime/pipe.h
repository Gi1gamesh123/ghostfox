#pragma once
#include <string>

namespace ruyijs {
struct PipeAsync;
enum class PipeResult { Pending, Ready, Closed, Failed };
class Pipe final {
 public:
  static constexpr unsigned kMaxFrame = 16 * 1024 * 1024;
  static constexpr unsigned kMaxQueuedFrames = 256;
  static constexpr unsigned kMaxQueuedBytes = kMaxFrame + 4;
  static constexpr bool ValidFrameSize(size_t size) { return size && size <= kMaxFrame; }
  Pipe() = default;
  ~Pipe();
  Pipe(const Pipe&) = delete;
  Pipe& operator=(const Pipe&) = delete;
  bool Connect(const char* name);
  // Server side of the same frame protocol, in two steps so the owner can
  // create the pipe, spawn the child, then wait: Listen creates the pipe and
  // Accept blocks until the one child connects. A page Context uses this to
  // reach a worker process it spawned.
  bool Listen(const char* name);
  bool Accept();
  bool Read(std::string* frame);
  bool Write(const std::string& frame);
  // Worker mode uses the same framing with stable OVERLAPPED buffers. Queue
  // and Poll never wait for a computing peer; one completed receive is enough
  // to apply backpressure. The default RPC methods above stay synchronous.
  bool ListenAsync(const char* name);
  bool ConnectAsync(const char* name);
  bool Queue(std::string frame);
  PipeResult Poll(std::string* frame, bool* received);
  // Submit pending writes without consuming a received frame or waiting.
  PipeResult Flush();
  size_t queuedBytes() const;
  size_t queuedFrames() const;
  bool connected() const;
  void Close();
 private:
  bool OpenClient(const char* name, bool asynchronous);
  bool OpenServer(const char* name, bool asynchronous);
  bool Transfer(void* data, unsigned length, bool writing);
  void* handle_ = nullptr;
  // stdio transport: reads come from stdin, writes go to stdout; the process
  // owns both ends' lifetime, so Close() leaves them alone.
  void* writeHandle_ = nullptr;
  bool stdio_ = false;
  PipeAsync* async_ = nullptr;
};
}

