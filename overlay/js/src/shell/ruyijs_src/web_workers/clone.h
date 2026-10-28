#ifndef RUYIJS_WEB_WORKERS_CLONE_H
#define RUYIJS_WEB_WORKERS_CLONE_H
#include <string>
#include <string_view>
#include "js/RootingAPI.h"
namespace ruyijs::web_workers {
bool WorkerTokenEnabled();
// Only engine-owned DifferentProcess data crosses the process boundary.
bool EncodeMessage(JSContext*, JS::HandleObject exception, JS::HandleValue value,
                   JS::HandleValue options, std::string* frame);
bool DecodeMessage(JSContext*, std::string_view frame, JS::MutableHandleValue value, char kind = '\x01');
struct WorkerMessageToken {
  uint64_t workerId = 0, sequence = 0;
  uint8_t direction = 0;
  explicit operator bool() const { return workerId && sequence && direction <= 1; }
};
bool EncodeMessageWithToken(JSContext*, JS::HandleObject exception, JS::HandleValue value,
                            JS::HandleValue options, const WorkerMessageToken&, std::string* frame);
bool DecodeMessageWithToken(JSContext*, std::string_view frame, WorkerMessageToken*,
                            JS::MutableHandleValue value);
// SharedWorker port frames: '\x08' opens a worker-side port (connect), '\x09'
// carries one message, '\x0a' closes a port. All three prefix a 4-byte little-
// endian port id; the message frame then carries the EncodeMessage payload
// without its leading '\x01' tag.
inline constexpr char kSharedConnect = '\x08', kSharedMessage = '\x09', kSharedClose = '\x0a';
bool EncodeSharedMessage(JSContext*, JS::HandleObject exception, uint32_t id,
                         JS::HandleValue value, JS::HandleValue transfer, std::string* frame);
bool DecodeSharedMessage(JSContext*, std::string_view frame, uint32_t* id,
                         JS::MutableHandleValue value);
inline bool EncodeSharedPortFrame(uint32_t id, char tag, std::string* frame) {
  frame->assign(1, tag);
  for (unsigned index = 0; index < 4; ++index) frame->push_back(char(id >> (8 * index)));
  return true;
}
inline bool DecodeSharedPortFrame(std::string_view frame, char tag, uint32_t* id) {
  if (frame.size() < 5 || frame[0] != tag) return false;
  *id = uint32_t(static_cast<unsigned char>(frame[1])) |
        (uint32_t(static_cast<unsigned char>(frame[2])) << 8) |
        (uint32_t(static_cast<unsigned char>(frame[3])) << 16) |
        (uint32_t(static_cast<unsigned char>(frame[4])) << 24);
  return true;
}
}
#endif
