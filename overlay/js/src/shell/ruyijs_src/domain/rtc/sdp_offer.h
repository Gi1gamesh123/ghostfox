#ifndef RUYIJS_DOMAIN_RTC_SDP_OFFER_H
#define RUYIJS_DOMAIN_RTC_SDP_OFFER_H
// Offer SDP for a peer connection that only carries a data channel, recorded
// from Firefox 158 (media.peerconnection enabled, no ICE agent). Pure values:
// the caller supplies the session id, the DTLS fingerprint and the ICE
// credentials, so this unit owns formatting only. Engine-independent by design.
#include <cstdint>
#include <string>

namespace ruyijs::domain::rtc {

// Firefox's default SCTP max-message-size when no value has been negotiated.
inline constexpr uint64_t kDefaultMaxMessageSize = 1073741823;

struct DataChannelOfferInput {
  std::string session_id;   // o= session id, 19 decimal digits
  std::string fingerprint;  // FormatFingerprint() of the 32-byte digest
  std::string ice_ufrag;    // 8 hex characters
  std::string ice_pwd;      // 32 hex characters
  uint64_t max_message_size = kDefaultMaxMessageSize;
};

// Offer for a peer with no transceivers and no data channel. Firefox still
// creates the session-level identity and ICE attributes, but no m-section.
inline std::string EmptyOffer(const DataChannelOfferInput& input) {
  std::string sdp;
  sdp += "v=0\r\n";
  sdp += "o=mozilla...THIS_IS_SDPARTA-99.0 ";
  sdp += input.session_id;
  sdp += " 0 IN IP4 0.0.0.0\r\n";
  sdp += "s=-\r\n";
  sdp += "t=0 0\r\n";
  sdp += "a=fingerprint:sha-256 ";
  sdp += input.fingerprint;
  sdp += "\r\n";
  sdp += "a=ice-options:trickle\r\n";
  sdp += "a=msid-semantic:WMS *\r\n";
  return sdp;
}

// One BUNDLE group and one SCTP application m-section, with no ICE candidates:
// Firefox gathers those separately and announces trickle ICE here.
inline std::string DataChannelOffer(const DataChannelOfferInput& input) {
  std::string sdp;
  sdp += "v=0\r\n";
  sdp += "o=mozilla...THIS_IS_SDPARTA-99.0 ";
  sdp += input.session_id;
  sdp += " 0 IN IP4 0.0.0.0\r\n";
  sdp += "s=-\r\n";
  sdp += "t=0 0\r\n";
  sdp += "a=fingerprint:sha-256 ";
  sdp += input.fingerprint;
  sdp += "\r\n";
  sdp += "a=group:BUNDLE 0\r\n";
  sdp += "a=ice-options:trickle\r\n";
  sdp += "a=msid-semantic:WMS *\r\n";
  sdp += "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\n";
  sdp += "c=IN IP4 0.0.0.0\r\n";
  sdp += "a=sendrecv\r\n";
  sdp += "a=ice-pwd:";
  sdp += input.ice_pwd;
  sdp += "\r\n";
  sdp += "a=ice-ufrag:";
  sdp += input.ice_ufrag;
  sdp += "\r\n";
  sdp += "a=mid:0\r\n";
  sdp += "a=setup:actpass\r\n";
  sdp += "a=sctp-port:5000\r\n";
  sdp += "a=max-message-size:";
  sdp += std::to_string(input.max_message_size);
  sdp += "\r\n";
  return sdp;
}

}  // namespace ruyijs::domain::rtc
#endif
