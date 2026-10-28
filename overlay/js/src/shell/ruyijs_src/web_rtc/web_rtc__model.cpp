#include "web_rtc/model.h"

#include "domain/rtc/rtc_session.h"

namespace ruyijs::web_rtc {
namespace {

namespace rtc = ruyijs::domain::rtc;

rtc::SessionDescription ToDomain(const DescriptionSpec& spec) {
  rtc::SessionDescription out;
  out.type = spec.type;
  out.sdp = spec.sdp;
  return out;
}

DescriptionSpec FromDomain(const rtc::SessionDescription& description) {
  DescriptionSpec out;
  out.type = description.type;
  out.sdp = description.sdp;
  return out;
}

rtc::DataChannelOptions ToDomain(const ChannelSpec& spec) {
  rtc::DataChannelOptions out;
  out.ordered = spec.ordered;
  out.negotiated = spec.negotiated;
  out.protocol = spec.protocol;
  if (spec.max_packet_lifetime >= 0) out.max_packet_lifetime = spec.max_packet_lifetime;
  if (spec.max_retransmits >= 0) out.max_retransmits = spec.max_retransmits;
  if (spec.id >= 0) out.id = spec.id;
  return out;
}

SetResult Convert(rtc::DescriptionStatus status) {
  switch (status) {
    case rtc::DescriptionStatus::kApplied: return SetResult::kApplied;
    case rtc::DescriptionStatus::kClosed: return SetResult::kClosed;
    case rtc::DescriptionStatus::kUnknownPeer: return SetResult::kUnknown;
    default: return SetResult::kInvalidState;
  }
}

}  // namespace

struct Model::Impl {
  rtc::RtcSessionAggregate aggregate;
};

Model::Model() noexcept : impl_(nullptr) {
  try {
    impl_ = new Impl();
  } catch (...) {
    impl_ = nullptr;
  }
}

Model::~Model() { delete impl_; }

uint32_t Model::create_peer() noexcept {
  if (!impl_) return 0;
  try {
    return impl_->aggregate.create_peer_connection();
  } catch (...) {
    return 0;
  }
}

bool Model::peer_info(uint32_t peer, PeerInfo* out) const noexcept {
  const auto* state = impl_ ? impl_->aggregate.find_peer(peer) : nullptr;
  if (!state) return false;
  out->found = true;
  out->closed = state->closed;
  out->signaling_state = state->signaling_state;
  out->ice_connection_state = state->ice_connection_state;
  out->ice_gathering_state = state->ice_gathering_state;
  out->connection_state = state->connection_state;
  out->has_local_description = state->local_description.has_value();
  out->has_remote_description = state->remote_description.has_value();
  out->data_transport_position = state->data_transport_position.value_or(UINT32_MAX);
  return true;
}

bool Model::local_description(uint32_t peer, DescriptionSpec* out) const noexcept {
  const auto* state = impl_ ? impl_->aggregate.find_peer(peer) : nullptr;
  if (!state || !state->local_description) return false;
  *out = FromDomain(*state->local_description);
  return true;
}

bool Model::remote_description(uint32_t peer, DescriptionSpec* out) const noexcept {
  const auto* state = impl_ ? impl_->aggregate.find_peer(peer) : nullptr;
  if (!state || !state->remote_description) return false;
  *out = FromDomain(*state->remote_description);
  return true;
}

uint32_t Model::create_channel(uint32_t peer, const ChannelSpec& spec) noexcept {
  if (!impl_) return 0;
  try {
    return impl_->aggregate.create_data_channel(peer, spec.label, ToDomain(spec));
  } catch (...) {
    return 0;
  }
}

bool Model::channel_info(uint32_t channel, ChannelInfo* out) const noexcept {
  const auto* state = impl_ ? impl_->aggregate.find_data_channel(channel) : nullptr;
  if (!state) return false;
  out->id = state->id;
  out->label = state->label;
  out->ready_state = state->ready_state;
  out->buffered_amount = state->buffered_amount;
  out->ordered = state->options.ordered;
  out->negotiated = state->options.negotiated;
  out->protocol = state->options.protocol;
  out->has_id = state->options.id.has_value();
  out->negotiated_id = state->options.id.value_or(0);
  out->has_max_retransmits = state->options.max_retransmits.has_value();
  out->max_retransmits = state->options.max_retransmits.value_or(0);
  out->has_max_packet_lifetime = state->options.max_packet_lifetime.has_value();
  out->max_packet_lifetime = state->options.max_packet_lifetime.value_or(0);
  return true;
}

bool Model::close_channel(uint32_t channel) noexcept {
  return impl_ && impl_->aggregate.close_data_channel(channel);
}

SendResult Model::send(uint32_t channel, uint64_t bytes) noexcept {
  if (!impl_) return SendResult::kUnknown;
  switch (impl_->aggregate.send_data(channel, bytes)) {
    case rtc::SendStatus::kSent: return SendResult::kSent;
    case rtc::SendStatus::kNotOpen: return SendResult::kNotOpen;
    default: return SendResult::kUnknown;
  }
}

uint32_t Model::add_transceiver(uint32_t peer, const std::string& kind,
                                const std::string& direction) noexcept {
  if (!impl_) return 0;
  try {
    return impl_->aggregate.add_transceiver(peer, kind, direction);
  } catch (...) {
    return 0;
  }
}

bool Model::transceiver_info(uint32_t transceiver, TransceiverInfo* out) const noexcept {
  const auto* state = impl_ ? impl_->aggregate.find_transceiver(transceiver) : nullptr;
  if (!state) return false;
  out->found = true;
  out->mid = state->mid;
  out->direction = state->direction;
  out->kind = state->kind;
  out->stopping = state->stopping;
  return true;
}

int Model::stop_transceiver(uint32_t transceiver) noexcept {
  return impl_ ? int(impl_->aggregate.stop_transceiver(transceiver)) : 1;
}

int Model::set_transceiver_direction(uint32_t transceiver, const std::string& direction) noexcept {
  try {
    return impl_ ? int(impl_->aggregate.set_transceiver_direction(transceiver, direction)) : 1;
  } catch (...) {
    return 1;
  }
}

bool Model::peer_channels(uint32_t peer, std::vector<uint32_t>* out) const noexcept {
  const auto* state = impl_ ? impl_->aggregate.find_peer(peer) : nullptr;
  if (!state) return false;
  out->assign(state->data_channels.begin(), state->data_channels.end());
  return true;
}

bool Model::peer_transceivers(uint32_t peer, std::vector<uint32_t>* out) const noexcept {
  const auto* state = impl_ ? impl_->aggregate.find_peer(peer) : nullptr;
  if (!state) return false;
  out->assign(state->transceivers.begin(), state->transceivers.end());
  return true;
}

DescriptionSpec Model::offer(uint32_t peer) const noexcept {
  if (!impl_) return DescriptionSpec{};
  return FromDomain(impl_->aggregate.create_offer(peer));
}

DescriptionSpec Model::answer(uint32_t peer) const noexcept {
  if (!impl_) return DescriptionSpec{};
  return FromDomain(impl_->aggregate.create_answer(peer));
}

SetResult Model::set_local(uint32_t peer, DescriptionSpec description) noexcept {
  if (!impl_) return SetResult::kUnknown;
  return Convert(impl_->aggregate.set_local_description(peer, ToDomain(description)));
}

SetResult Model::set_remote(uint32_t peer, DescriptionSpec description) noexcept {
  if (!impl_) return SetResult::kUnknown;
  return Convert(impl_->aggregate.set_remote_description(peer, ToDomain(description)));
}

CandidateResult Model::add_ice_candidate(uint32_t peer) noexcept {
  if (!impl_) return CandidateResult::kUnknown;
  switch (impl_->aggregate.add_ice_candidate(peer, rtc::CandidateState{})) {
    case rtc::CandidateStatus::kApplied: return CandidateResult::kApplied;
    case rtc::CandidateStatus::kClosed: return CandidateResult::kClosed;
    case rtc::CandidateStatus::kUnknownPeer: return CandidateResult::kUnknown;
    default: return CandidateResult::kNoRemoteDescription;
  }
}

bool Model::finish_gathering(uint32_t peer) noexcept {
  return impl_ && impl_->aggregate.finish_gathering(peer);
}
bool Model::close_peer(uint32_t peer) noexcept {
  return impl_ && impl_->aggregate.close_peer(peer);
}

}  // namespace ruyijs::web_rtc
