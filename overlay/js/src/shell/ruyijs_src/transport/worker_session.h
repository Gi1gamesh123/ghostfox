#pragma once
#include "transport/necko_channel.h"
#include "transport/worker_frame.h"
#include "transport/worker_schema.h"
#include <array>
#include <atomic>

namespace ruyijs::transport::worker {
struct SessionIdentity {
  Sender role = Sender::Parent;
  uint64_t epoch = 0;
  uint32_t peer_pid = 0;
  std::array<unsigned char, 16> nonce{};
  std::array<char, 64> manifest{};
};
// Channels outlive this object. Handshake is exclusive except Stop; after it,
// one reader and one writer may run concurrently. Stop/join before destruction.
class Session final {
 public:
  Session(NeckoChannel& control, NeckoChannel& body) noexcept : control_(control), body_(body) {}
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  JobStatus Handshake(const SessionIdentity& identity, uint64_t deadline) noexcept;
  // Payload JSON is parsed once by the W0 parser together with its envelope.
  // This does not allocate a second persistent JSON tree or mutate its caller.
  JobStatus Send(std::string_view type, std::string_view payload, uint64_t deadline) noexcept;
  JobStatus Receive(ControlDocument* output, uint64_t deadline) noexcept;
  JobStatus ReceiveNext(ControlDocument* output, uint32_t stall_ms = 30000) noexcept;
  void Stop() noexcept;
  bool valid() const noexcept { return ready_ && !failed_; }
 private:
  JobStatus Fail(JobStatus status) noexcept;
  JobStatus ReadMessage(ControlDocument* output, uint64_t deadline, uint32_t stall_ms = 0) noexcept;
  JobStatus WriteMessage(std::string_view type, std::string_view payload, uint64_t deadline) noexcept;
  JobStatus Prefaces(uint64_t deadline) noexcept;
  JobStatus Hello(uint64_t deadline) noexcept;
  NeckoChannel& control_;
  NeckoChannel& body_;
  SessionIdentity identity_{};
  uint32_t pid_ = 0;
  uint64_t frequency_ = 0, send_seq_ = 0, receive_seq_ = 0;
  std::atomic<bool> started_{false}, ready_{false}, failed_{false};
  std::atomic<bool> reading_{false}, writing_{false};
};
}
