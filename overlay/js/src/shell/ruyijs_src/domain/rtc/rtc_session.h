#pragma once

#include "domain/shared/aggregate_root.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ruyijs::domain::rtc {

using PeerConnectionId = uint32_t;
using DataChannelId = uint32_t;

struct SessionDescription {
    std::string type;
    std::string sdp;
};

// Outcome of a description setter. The aggregate owns the signaling
// transition table; the adapter only maps the status onto a JS error.
enum class DescriptionStatus {
    kApplied,
    kUnknownPeer,
    kClosed,
    kInvalidState,
};

// addIceCandidate only reaches the aggregate once the JS dictionary has been
// validated; the remaining case the aggregate owns is a missing remote
// description.
enum class CandidateStatus {
    kApplied,
    kUnknownPeer,
    kClosed,
    kNoRemoteDescription,
};

// send() only succeeds on an open channel.
enum class SendStatus {
    kSent,
    kUnknownChannel,
    kNotOpen,
};

struct CandidateState {
    std::string candidate;
    std::string sdp_mid;
    std::optional<int> sdp_m_line_index;
    std::optional<std::string> username_fragment;
};

using TransceiverId = uint32_t;

struct TransceiverState {
    TransceiverId id = 0;
    PeerConnectionId peer_id = 0;
    std::string mid;
    std::string direction = "sendrecv";
    std::string kind;
    // stop() or the peer's close(): [[Stopping]]. [[Stopped]] needs a
    // negotiation, which this aggregate does not run.
    bool stopping = false;
};
enum class TransceiverStatus { kApplied, kUnknown, kClosed, kStopping };

struct DataChannelOptions {
    bool ordered = true;
    std::optional<int> max_packet_lifetime;
    std::optional<int> max_retransmits;
    std::string protocol;
    bool negotiated = false;
    std::optional<int> id;
};

struct DataChannelState {
    DataChannelId id = 0;
    PeerConnectionId peer_id = 0;
    std::string label;
    DataChannelOptions options;
    std::string ready_state = "connecting";
    uint64_t buffered_amount = 0;
};

struct PeerConnectionState {
    PeerConnectionId id = 0;
    std::string signaling_state = "stable";
    std::string ice_connection_state = "new";
    std::string ice_gathering_state = "new";
    std::string connection_state = "new";
    std::optional<SessionDescription> local_description;
    std::optional<SessionDescription> remote_description;
    std::vector<DataChannelId> data_channels;
    std::vector<TransceiverId> transceivers;
    std::optional<uint32_t> data_transport_position;
    bool closed = false;
};

class RtcSessionAggregate : public shared::AggregateRoot<uint64_t> {
public:
    explicit RtcSessionAggregate(uint64_t id = 1)
        : AggregateRoot<uint64_t>(id) {}

    PeerConnectionId create_peer_connection() {
        const auto id = next_peer_id_++;
        peers_.emplace(id, PeerConnectionState{id});
        return id;
    }

    const PeerConnectionState* find_peer(PeerConnectionId id) const noexcept {
        const auto it = peers_.find(id);
        return it == peers_.end() ? nullptr : &it->second;
    }

    PeerConnectionState* find_peer(PeerConnectionId id) noexcept {
        const auto it = peers_.find(id);
        return it == peers_.end() ? nullptr : &it->second;
    }

    DataChannelId create_data_channel(PeerConnectionId peer_id,
                                      std::string label,
                                      DataChannelOptions options) {
        auto* peer = find_peer(peer_id);
        if (!peer || peer->closed) return 0;
        const auto id = next_channel_id_++;
        channels_.emplace(id, DataChannelState{
            id, peer_id, std::move(label), std::move(options)});
        peer->data_channels.push_back(id);
        if (!peer->data_transport_position)
            peer->data_transport_position = uint32_t(peer->transceivers.size());
        return id;
    }

    const DataChannelState* find_data_channel(DataChannelId id) const noexcept {
        const auto it = channels_.find(id);
        return it == channels_.end() ? nullptr : &it->second;
    }

    bool close_data_channel(DataChannelId id) noexcept {
        const auto it = channels_.find(id);
        if (it == channels_.end()) return false;
        it->second.ready_state = "closed";
        it->second.buffered_amount = 0;
        return true;
    }

    SessionDescription create_offer(PeerConnectionId peer_id) const {
        return find_peer(peer_id)
                   ? SessionDescription{"offer", "v=0\r\n"}
                   : SessionDescription{};
    }

    SessionDescription create_answer(PeerConnectionId peer_id) const {
        return find_peer(peer_id)
                   ? SessionDescription{"answer", "v=0\r\n"}
                   : SessionDescription{};
    }

    TransceiverId add_transceiver(PeerConnectionId peer_id, std::string kind,
                                  std::string direction) {
        auto* peer = find_peer(peer_id);
        if (!peer || peer->closed) return 0;
        const auto id = next_transceiver_id_++;
        TransceiverState entry;
        entry.id = id;
        entry.peer_id = peer_id;
        entry.direction = std::move(direction);
        entry.kind = std::move(kind);
        transceivers_.emplace(id, std::move(entry));
        peer->transceivers.push_back(id);
        return id;
    }

    const TransceiverState* find_transceiver(TransceiverId id) const noexcept {
        const auto it = transceivers_.find(id);
        return it == transceivers_.end() ? nullptr : &it->second;
    }

    DescriptionStatus set_local_description(PeerConnectionId id,
                                            SessionDescription description) {
        auto* peer = find_peer(id);
        if (!peer) return DescriptionStatus::kUnknownPeer;
        if (peer->closed) return DescriptionStatus::kClosed;
        if (!advance(peer, description.type, false))
            return DescriptionStatus::kInvalidState;
        peer->local_description = std::move(description);
        // The first local description starts ICE gathering; the adapter's last
        // candidate step completes it (finish_gathering).
        if (peer->ice_gathering_state == "new") peer->ice_gathering_state = "gathering";
        settle(peer);
        return DescriptionStatus::kApplied;
    }

    DescriptionStatus set_remote_description(PeerConnectionId id,
                                             SessionDescription description) {
        auto* peer = find_peer(id);
        if (!peer) return DescriptionStatus::kUnknownPeer;
        if (peer->closed) return DescriptionStatus::kClosed;
        if (!advance(peer, description.type, true))
            return DescriptionStatus::kInvalidState;
        peer->remote_description = std::move(description);
        settle(peer);
        return DescriptionStatus::kApplied;
    }

    // The dictionary-level checks (candidate present, one of
    // sdpMid/sdpMLineIndex non-null) happen in the adapter; the aggregate owns
    // the remote-description precondition.
    CandidateStatus add_ice_candidate(PeerConnectionId id,
                                      CandidateState candidate) {
        auto* peer = find_peer(id);
        if (!peer) return CandidateStatus::kUnknownPeer;
        if (peer->closed) return CandidateStatus::kClosed;
        if (!peer->remote_description) return CandidateStatus::kNoRemoteDescription;
        (void)candidate;
        return CandidateStatus::kApplied;
    }

    // No transport exists, so an accepted send only queues bytes on the
    // channel; close() discards them.
    SendStatus send_data(DataChannelId id, uint64_t bytes) {
        const auto it = channels_.find(id);
        if (it == channels_.end()) return SendStatus::kUnknownChannel;
        if (it->second.ready_state != "open") return SendStatus::kNotOpen;
        it->second.buffered_amount += bytes;
        return SendStatus::kSent;
    }

    bool finish_gathering(PeerConnectionId id) noexcept {
        auto* peer = find_peer(id);
        if (!peer || peer->closed || peer->ice_gathering_state != "gathering") return false;
        peer->ice_gathering_state = "complete";
        return true;
    }
    // RTCRtpTransceiver.stop(): "stop sending and receiving" (direction stopped).
    TransceiverStatus stop_transceiver(TransceiverId id) noexcept {
        auto it = transceivers_.find(id);
        if (it == transceivers_.end()) return TransceiverStatus::kUnknown;
        const auto* peer = find_peer(it->second.peer_id);
        if (!peer || peer->closed) return TransceiverStatus::kClosed;
        it->second.stopping = true;
        it->second.direction = "stopped";
        return TransceiverStatus::kApplied;
    }
    // RTCRtpTransceiver.direction setter, after the binding rejected "stopped".
    TransceiverStatus set_transceiver_direction(TransceiverId id, std::string direction) noexcept {
        auto it = transceivers_.find(id);
        if (it == transceivers_.end()) return TransceiverStatus::kUnknown;
        if (it->second.stopping) return TransceiverStatus::kStopping;
        it->second.direction = std::move(direction);
        return TransceiverStatus::kApplied;
    }
    bool close_peer(PeerConnectionId id) noexcept {
        auto* peer = find_peer(id);
        if (!peer) return false;
        // Closing stops every transceiver (RTCPeerConnection.close step "stop").
        for (const auto transceiver : peer->transceivers) {
            auto it = transceivers_.find(transceiver);
            if (it != transceivers_.end()) it->second.stopping = true;
        }
        peer->closed = true;
        peer->signaling_state = "closed";
        peer->ice_connection_state = "closed";
        peer->connection_state = "closed";
        for (const auto channel : peer->data_channels) close_data_channel(channel);
        return true;
    }

private:
    // WebRTC signaling table: a description type that is illegal in the
    // current state is rejected without mutating anything (InvalidStateError).
    static bool advance(PeerConnectionState* peer, const std::string& type,
                        bool remote) {
        const std::string& state = peer->signaling_state;
        if (type == "offer") {
            if (remote) {
                if (state == "stable" || state == "have-local-offer" ||
                    state == "have-remote-pranswer") {
                    peer->signaling_state = "have-remote-offer";
                    return true;
                }
            } else if (state == "stable" || state == "have-remote-offer" ||
                       state == "have-local-pranswer") {
                peer->signaling_state = "have-local-offer";
                return true;
            }
            return false;
        }
        if (type == "pranswer") {
            if (remote) {
                if (state == "have-local-offer" ||
                    state == "have-remote-pranswer") {
                    peer->signaling_state = "have-remote-pranswer";
                    return true;
                }
            } else if (state == "have-remote-offer" ||
                       state == "have-local-pranswer") {
                peer->signaling_state = "have-local-pranswer";
                return true;
            }
            return false;
        }
        if (type == "answer") {
            if (remote) {
                if (state == "have-local-offer" ||
                    state == "have-remote-pranswer") {
                    peer->signaling_state = "stable";
                    return true;
                }
            } else if (state == "have-remote-offer" ||
                       state == "have-local-pranswer") {
                peer->signaling_state = "stable";
                return true;
            }
            return false;
        }
        return false;
    }

    // With both descriptions present the transportless session is connected:
    // there is no ICE agent, so the peer reaches its terminal values in this
    // step and every connecting data channel opens.
    void settle(PeerConnectionState* peer) {
        if (!peer->local_description || !peer->remote_description) return;
        peer->ice_connection_state = "connected";
        peer->connection_state = "connected";
        for (const auto id : peer->data_channels) {
            const auto it = channels_.find(id);
            if (it != channels_.end() && it->second.ready_state == "connecting")
                it->second.ready_state = "open";
        }
    }

    PeerConnectionId next_peer_id_ = 1;
    DataChannelId next_channel_id_ = 1;
    TransceiverId next_transceiver_id_ = 1;
    std::unordered_map<PeerConnectionId, PeerConnectionState> peers_;
    std::unordered_map<DataChannelId, DataChannelState> channels_;
    std::unordered_map<TransceiverId, TransceiverState> transceivers_;
};

} // namespace ruyijs::domain::rtc
