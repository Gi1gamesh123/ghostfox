// Service Worker frame protocol between a Window realm's registry and its service
// worker process (docs/owners/SERVICE_WORKER.md). Frames ride the Web Workers pipe;
// that owner only transports them. Structured-clone payloads use the Worker codec.
#ifndef RUYIJS_WEB_SERVICE_WORKER_PROTOCOL_H
#define RUYIJS_WEB_SERVICE_WORKER_PROTOCOL_H
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ruyijs::web_service_worker::protocol {
// Parent -> service worker.
inline constexpr char kLifecycle = '\x10';     // name ("install" | "activate")
inline constexpr char kMessage = '\x11';       // client record, origin, then a clone frame
inline constexpr char kClients = '\x12';       // count, client records
inline constexpr char kRegistration = '\x13';  // scope, scriptURL, updateViaCache, 3 worker states
// Service worker -> parent.
inline constexpr char kEvaluated = '\x18';     // the script ran; install may start
inline constexpr char kLifecycleDone = '\x19'; // name, "1" | "0"
inline constexpr char kClientMessage = '\x1a'; // client id, then a clone frame
inline constexpr char kClaim = '\x1b';
inline constexpr char kEvaluationFailed = '\x1d';  // the script did not compile or threw (load failure: '\x04')

struct Client {
  std::string id, url, frameType, visibility;
  bool focused = false, controlled = false;
};

class Writer {
 public:
  explicit Writer(char tag) { frame_.push_back(tag); }
  Writer& Field(std::string_view value) {
    const uint32_t size = uint32_t(value.size());
    for (unsigned i = 0; i < 4; ++i) frame_.push_back(char(size >> (8 * i)));
    frame_.append(value);
    return *this;
  }
  Writer& Record(const Client& c) {
    return Field(c.id).Field(c.url).Field(c.frameType).Field(c.visibility)
        .Field(c.focused ? "1" : "0").Field(c.controlled ? "1" : "0");
  }
  std::string Take(std::string_view tail = {}) { frame_.append(tail); return std::move(frame_); }
 private:
  std::string frame_;
};

class Reader {
 public:
  explicit Reader(std::string_view frame) : rest_(frame.substr(frame.empty() ? 0 : 1)) {}
  bool Field(std::string* out) {
    if (rest_.size() < 4) return false;
    uint32_t size = 0;
    for (unsigned i = 0; i < 4; ++i) size |= uint32_t(static_cast<unsigned char>(rest_[i])) << (8 * i);
    if (rest_.size() - 4 < size) return false;
    out->assign(rest_.substr(4, size));
    rest_.remove_prefix(4 + size);
    return true;
  }
  bool Record(Client* c) {
    std::string focused, controlled;
    if (!Field(&c->id) || !Field(&c->url) || !Field(&c->frameType) || !Field(&c->visibility) ||
        !Field(&focused) || !Field(&controlled)) return false;
    c->focused = focused == "1";
    c->controlled = controlled == "1";
    return true;
  }
  std::string_view Rest() const { return rest_; }
 private:
  std::string_view rest_;
};
}  // namespace ruyijs::web_service_worker::protocol
#endif
