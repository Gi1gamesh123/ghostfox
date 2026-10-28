#include "runtime/pipe_async.h"
#ifdef _WIN32

namespace ruyijs {
PipeResult PipeError(DWORD error) {
  return error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA ||
      error == ERROR_PIPE_NOT_CONNECTED || error == ERROR_OPERATION_ABORTED
      ? PipeResult::Closed : PipeResult::Failed;
}
void CancelPipeOperation(HANDLE pipe, PipeOperation& operation) {
  if (!operation.pending) return;
  DWORD ignored = 0;
  // Cancellation completion owns the lifetime of the buffers and OVERLAPPED.
  GetOverlappedResult(pipe, &operation.value, &ignored, TRUE);
  operation.pending = false;
}
bool Pipe::ListenAsync(const char* name) {
  if (async_ || !OpenServer(name, true)) return false;
  async_ = NewPipeAsync();
  if (!async_) { Close(); return false; }
  async_->server = true;
  return true;
}
bool Pipe::ConnectAsync(const char* name) {
  if (async_ || !OpenClient(name, true)) return false;
  async_ = NewPipeAsync();
  if (!async_) { Close(); return false; }
  async_->connected = true;
  return true;
}
PipeResult PipeAccept(HANDLE pipe, PipeAsync& state) {
  if (state.connected) return PipeResult::Ready;
  auto& op = state.accept;
  if (!op.pending) {
    if (ConnectNamedPipe(pipe, &op.value)) { state.connected = true; return PipeResult::Ready; }
    const auto error = GetLastError();
    if (error == ERROR_PIPE_CONNECTED) { state.connected = true; return PipeResult::Ready; }
    if (error != ERROR_IO_PENDING) return PipeError(error);
    op.pending = true;
  }
  DWORD transferred = 0;
  if (!GetOverlappedResult(pipe, &op.value, &transferred, FALSE)) {
    const auto error = GetLastError();
    if (error == ERROR_IO_INCOMPLETE) return PipeResult::Pending;
    op.pending = false;
    return PipeError(error);
  }
  op.pending = false; state.connected = true;
  return PipeResult::Ready;
}
PipeResult PipeIo(HANDLE pipe, PipeOperation& op, bool writing,
                    void* bytes, DWORD size, DWORD* transferred) {
  *transferred = 0;
  if (!op.pending) {
    const HANDLE event = op.value.hEvent;
    op.value = {}; op.value.hEvent = event;
    ResetEvent(event);
    const bool done = writing ? WriteFile(pipe, bytes, size, nullptr, &op.value)
                              : ReadFile(pipe, bytes, size, nullptr, &op.value);
    if (!done && GetLastError() != ERROR_IO_PENDING) return PipeError(GetLastError());
    op.pending = true;
  }
  if (!GetOverlappedResult(pipe, &op.value, transferred, FALSE)) {
    const auto error = GetLastError();
    if (error == ERROR_IO_INCOMPLETE) return PipeResult::Pending;
    op.pending = false;
    return PipeError(error);
  }
  op.pending = false;
  return *transferred ? PipeResult::Ready : PipeResult::Closed;
}

}  // namespace ruyijs
#endif
