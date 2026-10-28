// Firefox offer media sections (docs/owners/WEB_RTC.md, 2026-10-21): audio and
// video m-lines in Firefox 158's attribute order and codec set, then the data
// channel m-line, all bundled. Engine-free.
#ifndef RUYIJS_DOMAIN_RTC_SDP_MEDIA_H
#define RUYIJS_DOMAIN_RTC_SDP_MEDIA_H
#include <cstdint>
#include <string>
#include <vector>
#include "domain/rtc/sdp_offer.h"

namespace ruyijs::domain::rtc {
struct MediaSectionInput {
  std::string kind;       // "audio" or "video"
  std::string direction;  // sendrecv/sendonly/recvonly/inactive
  uint32_t ssrc = 0;
};
inline void AppendLines(std::string* sdp, std::initializer_list<const char*> lines) {
  for (const char* line : lines) { *sdp += line; *sdp += "\r\n"; }
}
inline void AppendIce(std::string* sdp, const DataChannelOfferInput& input, size_t mid) {
  *sdp += "a=ice-pwd:" + input.ice_pwd + "\r\na=ice-ufrag:" + input.ice_ufrag + "\r\na=mid:" + std::to_string(mid) + "\r\n";
}
inline void AudioSection(std::string* sdp, const DataChannelOfferInput& input, const MediaSectionInput& media,
                         size_t mid, const std::string& cname) {
  AppendLines(sdp, {"m=audio 9 UDP/TLS/RTP/SAVPF 109 9 0 8 101", "c=IN IP4 0.0.0.0"});
  *sdp += "a=" + media.direction + "\r\n";
  AppendLines(sdp, {"a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level",
                    "a=extmap:2/recvonly urn:ietf:params:rtp-hdrext:csrc-audio-level",
                    "a=extmap:3 urn:ietf:params:rtp-hdrext:sdes:mid",
                    "a=extmap:7 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01",
                    "a=extmap-allow-mixed", "a=fmtp:109 maxplaybackrate=48000;stereo=1;useinbandfec=1",
                    "a=fmtp:101 0-15"});
  AppendIce(sdp, input, mid);
  AppendLines(sdp, {"a=rtcp-fb:109 transport-cc", "a=rtcp-fb:9 transport-cc", "a=rtcp-fb:0 transport-cc",
                    "a=rtcp-fb:8 transport-cc", "a=rtcp-mux", "a=rtpmap:109 opus/48000/2", "a=rtpmap:9 G722/8000/1",
                    "a=rtpmap:0 PCMU/8000", "a=rtpmap:8 PCMA/8000", "a=rtpmap:101 telephone-event/8000/1",
                    "a=setup:actpass"});
  *sdp += "a=ssrc:" + std::to_string(media.ssrc) + " cname:" + cname + "\r\n";
}
inline void VideoSection(std::string* sdp, const DataChannelOfferInput& input, const MediaSectionInput& media,
                         size_t mid, const std::string& cname) {
  // A sending video section omits the H.264 packetization-mode=0 pairs (97/98, 103/104), as Firefox does.
  const bool receiveOnly = media.direction == "recvonly";
  *sdp += receiveOnly ? "m=video 9 UDP/TLS/RTP/SAVPF 120 124 121 125 126 127 97 98 105 106 103 104 99 100 123 122 119\r\n"
                      : "m=video 9 UDP/TLS/RTP/SAVPF 120 124 121 125 126 127 105 106 99 100 123 122 119\r\n";
  *sdp += "c=IN IP4 0.0.0.0\r\na=" + media.direction + "\r\n";
  AppendLines(sdp, {"a=extmap:3 urn:ietf:params:rtp-hdrext:sdes:mid",
                    "a=extmap:4 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
                    "a=extmap:5 urn:ietf:params:rtp-hdrext:toffset",
                    "a=extmap:6/recvonly http://www.webrtc.org/experiments/rtp-hdrext/playout-delay",
                    "a=extmap:7 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01",
                    "a=extmap:8 urn:3gpp:video-orientation",
                    "a=extmap:9/recvonly https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension",
                    "a=extmap-allow-mixed",
                    "a=fmtp:126 profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1"});
  if (receiveOnly) AppendLines(sdp, {"a=fmtp:97 profile-level-id=42e01f;level-asymmetry-allowed=1"});
  AppendLines(sdp, {"a=fmtp:105 profile-level-id=42001f;level-asymmetry-allowed=1;packetization-mode=1"});
  if (receiveOnly) AppendLines(sdp, {"a=fmtp:103 profile-level-id=42001f;level-asymmetry-allowed=1"});
  AppendLines(sdp, {"a=fmtp:99 profile=0;level-idx=9;tier=0", "a=fmtp:120 max-fs=12288;max-fr=60", "a=fmtp:124 apt=120",
                    "a=fmtp:121 max-fs=12288;max-fr=60", "a=fmtp:125 apt=121", "a=fmtp:127 apt=126"});
  if (receiveOnly) AppendLines(sdp, {"a=fmtp:98 apt=97"});
  AppendLines(sdp, {"a=fmtp:106 apt=105"});
  if (receiveOnly) AppendLines(sdp, {"a=fmtp:104 apt=103"});
  AppendLines(sdp, {"a=fmtp:100 apt=99", "a=fmtp:119 apt=122"});
  AppendIce(sdp, input, mid);
  std::vector<const char*> feedback = {"120", "121", "126"};
  if (receiveOnly) feedback.push_back("97");
  feedback.push_back("105");
  if (receiveOnly) feedback.push_back("103");
  for (const char* pt : {"99", "123", "122"}) feedback.push_back(pt);
  for (const char* pt : feedback)
    for (const char* kind : {" nack", " nack pli", " ccm fir", " goog-remb", " transport-cc"})
      *sdp += std::string("a=rtcp-fb:") + pt + kind + "\r\n";
  AppendLines(sdp, {"a=rtcp-mux", "a=rtcp-rsize", "a=rtpmap:120 VP8/90000", "a=rtpmap:124 rtx/90000",
                    "a=rtpmap:121 VP9/90000", "a=rtpmap:125 rtx/90000", "a=rtpmap:126 H264/90000",
                    "a=rtpmap:127 rtx/90000"});
  if (receiveOnly) AppendLines(sdp, {"a=rtpmap:97 H264/90000", "a=rtpmap:98 rtx/90000"});
  AppendLines(sdp, {"a=rtpmap:105 H264/90000", "a=rtpmap:106 rtx/90000"});
  if (receiveOnly) AppendLines(sdp, {"a=rtpmap:103 H264/90000", "a=rtpmap:104 rtx/90000"});
  AppendLines(sdp, {"a=rtpmap:99 AV1/90000", "a=rtpmap:100 rtx/90000", "a=rtpmap:123 ulpfec/90000",
                    "a=rtpmap:122 red/90000", "a=rtpmap:119 rtx/90000", "a=setup:actpass"});
  *sdp += "a=ssrc:" + std::to_string(media.ssrc) + " cname:" + cname + "\r\n";
}
// The whole offer: one m-line per transceiver, then the data channel's.
inline std::string MediaOffer(const DataChannelOfferInput& input, const std::vector<MediaSectionInput>& media,
                              bool dataChannel, const std::string& cname) {
  const size_t count = media.size() + (dataChannel ? 1 : 0);
  std::string sdp = "v=0\r\no=mozilla...THIS_IS_SDPARTA-99.0 " + input.session_id + " 0 IN IP4 0.0.0.0\r\ns=-\r\nt=0 0\r\n";
  sdp += "a=fingerprint:sha-256 " + input.fingerprint + "\r\na=group:BUNDLE";
  for (size_t mid = 0; mid < count; ++mid) sdp += " " + std::to_string(mid);
  sdp += "\r\na=ice-options:trickle\r\na=msid-semantic:WMS *\r\n";
  for (size_t mid = 0; mid < media.size(); ++mid) {
    if (media[mid].kind == "audio") AudioSection(&sdp, input, media[mid], mid, cname);
    else VideoSection(&sdp, input, media[mid], mid, cname);
  }
  if (dataChannel) {
    sdp += "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\nc=IN IP4 0.0.0.0\r\na=sendrecv\r\n";
    AppendIce(&sdp, input, media.size());
    sdp += "a=setup:actpass\r\na=sctp-port:5000\r\na=max-message-size:" + std::to_string(input.max_message_size) + "\r\n";
  }
  return sdp;
}
}  // namespace ruyijs::domain::rtc
#endif
