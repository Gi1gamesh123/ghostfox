#include "web_workers/child.h"
#include "web_workers/service.h"
#include "web_workers/clone.h"
#include "runtime/pipe.h"
#include "jsapi.h"
#include <windows.h>

namespace ruyijs::web_workers {
PipeResult Child::NextFrame(std::string* frame, bool* received) {
  if (deferred_.empty()) return pipe_.Poll(frame, received);
  *frame = std::move(deferred_.front());
  deferred_.pop_front();
  deferredBytes_ -= frame->size();
  *received = true;
  return PipeResult::Ready;
}
bool Child::Exchange(std::string_view request, std::string* reply) {
  if (request.size() >= Pipe::kMaxFrame)
    return WebKit::Fail(cx_, "Worker service request exceeds the frame limit");
  std::string outgoing(1, kServiceRequest);
  outgoing.append(request);
  if (!pipe_.Queue(std::move(outgoing)))
    return WebKit::Fail(cx_, "Worker service queue is full or closed");
  const ULONGLONG deadline = GetTickCount64() + 30000;
  while (GetTickCount64() < deadline) {
    std::string frame;
    bool received = false;
    const auto status = pipe_.Poll(&frame, &received);
    if (received) {
      if (!frame.empty() && frame[0] == kServiceReply) {
        reply->assign(frame.data() + 1, frame.size() - 1);
        return true;
      }
      // A synchronous native method must not dispatch a nested event: messages
      // and service worker frames wait for the serve loop.
      const bool deferrable = !frame.empty() &&
          (frame[0] == '\x01' || frame[0] == '\x07' || frame[0] == kSharedConnect ||
           frame[0] == kSharedMessage || frame[0] == kSharedClose || (service() && frame[0] >= '\x10'));
      if (!deferrable || deferred_.size() >= Pipe::kMaxQueuedFrames ||
          frame.size() > Pipe::kMaxFrame - deferredBytes_) break;
      deferredBytes_ += frame.size();
      deferred_.push_back(std::move(frame));
    } else if (status == PipeResult::Closed || status == PipeResult::Failed) break;
    else Sleep(2);
  }
  closed_ = true;
  return WebKit::Fail(cx_, "Worker service channel closed, exceeded its queue, or timed out");
}
}
