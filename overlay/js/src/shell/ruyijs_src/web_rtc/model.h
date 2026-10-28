#ifndef RUYIJS_WEB_RTC_MODEL_H
#define RUYIJS_WEB_RTC_MODEL_H
#include <cstdint>
#include <string>
#include <vector>

namespace ruyijs::web_rtc {

// Plain mirrors of the imported ruyijs RTC owner. model.cpp is the only unit
// that sees domain/rtc/rtc_session.h; every other web_rtc unit speaks these
// types and the noexcept facade below.
struct DescriptionSpec {
  std::string type;
  std::string sdp;
};

struct ChannelSpec {
  std::string label;
  bool ordered = true;
  bool negotiated = false;
  std::string protocol;
  int32_t id = -1;                 // -1 means the dictionary omitted it
  int32_t max_retransmits = -1;
  int32_t max_packet_lifetime = -1;
};

struct ChannelInfo {
  uint32_t id = 0;
  std::string label;
  std::string ready_state;
  uint64_t buffered_amount = 0;
  bool ordered = true;
  bool negotiated = false;
  std::string protocol;
  bool has_id = false;
  int32_t negotiated_id = 0;
  bool has_max_retransmits = false;
  int32_t max_retransmits = 0;
  bool has_max_packet_lifetime = false;
  int32_t max_packet_lifetime = 0;
};

struct PeerInfo {
  bool found = false;
  bool closed = false;
  std::string signaling_state;
  std::string ice_connection_state;
  std::string ice_gathering_state;
  std::string connection_state;
  bool has_local_description = false;
  bool has_remote_description = false;
  uint32_t data_transport_position = UINT32_MAX;
};

struct TransceiverInfo {
  bool found = false;
  std::string mid;
  std::string direction;
  std::string kind;
  bool stopping = false;
};

enum class SetResult { kApplied, kUnknown, kClosed, kInvalidState };
enum class CandidateResult { kApplied, kUnknown, kClosed, kNoRemoteDescription };
enum class SendResult { kSent, kUnknown, kNotOpen };

// noexcept facade over ruyijs::domain::rtc::RtcSessionAggregate. One Model lives
// per browsing realm, so separate pages and iframes never share peer state.
class Model {
 public:
  Model() noexcept;
  ~Model();
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  uint32_t create_peer() noexcept;  // 0 when the aggregate cannot be built
  bool peer_info(uint32_t peer, PeerInfo* out) const noexcept;
  bool local_description(uint32_t peer, DescriptionSpec* out) const noexcept;
  bool remote_description(uint32_t peer, DescriptionSpec* out) const noexcept;

  uint32_t create_channel(uint32_t peer, const ChannelSpec& spec) noexcept;
  bool channel_info(uint32_t channel, ChannelInfo* out) const noexcept;
  bool close_channel(uint32_t channel) noexcept;
  SendResult send(uint32_t channel, uint64_t bytes) noexcept;

  uint32_t add_transceiver(uint32_t peer, const std::string& kind,
                           const std::string& direction) noexcept;
  bool transceiver_info(uint32_t transceiver, TransceiverInfo* out) const noexcept;
  // 0 applied, 1 unknown, 2 peer closed, 3 transceiver stopping.
  int stop_transceiver(uint32_t transceiver) noexcept;
  int set_transceiver_direction(uint32_t transceiver, const std::string& direction) noexcept;

  bool peer_channels(uint32_t peer, std::vector<uint32_t>* out) const noexcept;
  bool peer_transceivers(uint32_t peer, std::vector<uint32_t>* out) const noexcept;

  DescriptionSpec offer(uint32_t peer) const noexcept;
  DescriptionSpec answer(uint32_t peer) const noexcept;
  SetResult set_local(uint32_t peer, DescriptionSpec description) noexcept;
  SetResult set_remote(uint32_t peer, DescriptionSpec description) noexcept;
  CandidateResult add_ice_candidate(uint32_t peer) noexcept;
  bool close_peer(uint32_t peer) noexcept;
  bool finish_gathering(uint32_t peer) noexcept;

 private:
  struct Impl;
  Impl* impl_;
};

}  // namespace ruyijs::web_rtc
#endif
