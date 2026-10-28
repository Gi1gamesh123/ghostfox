#pragma once
#include <cstdint>
#include <span>

namespace ruyijs::domain::rtc {
// Recorded Firefox 157 Windows default environment. A capability advertisement
// does not run an encoder; hardware/GMP/pref profile adaptation is separate.
enum class RtpKind { Audio, Video };
enum class RtpDirection { Send, Receive };
struct RtpCodec {
  const char* mime;
  uint32_t clock;
  uint16_t channels;
  const char* fmtp;
  bool receive_only = false;
  constexpr bool Supports(RtpDirection direction) const {
    return direction == RtpDirection::Receive || !receive_only;
  }
};
struct RtpExtension {
  const char* uri;
  bool receive_only = false;
  constexpr bool Supports(RtpDirection direction) const {
    return direction == RtpDirection::Receive || !receive_only;
  }
};
inline constexpr RtpCodec kDefaultAudioCodecs[] = {
  {"audio/opus", 48000, 2, "maxplaybackrate=48000;stereo=1;useinbandfec=1"},
  {"audio/G722", 8000, 1, nullptr},
  {"audio/PCMU", 8000, 1, nullptr},
  {"audio/PCMA", 8000, 1, nullptr},
  {"audio/telephone-event", 8000, 1, "0-15"},
};
inline constexpr RtpCodec kDefaultVideoCodecs[] = {
  {"video/VP8", 90000, 0, "max-fs=12288;max-fr=60"},
  {"video/rtx", 90000, 0, nullptr},
  {"video/VP9", 90000, 0, "max-fs=12288;max-fr=60"},
  {"video/H264", 90000, 0, "profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1"},
  {"video/H264", 90000, 0, "profile-level-id=42e01f;level-asymmetry-allowed=1", true},
  {"video/H264", 90000, 0, "profile-level-id=42001f;level-asymmetry-allowed=1;packetization-mode=1"},
  {"video/H264", 90000, 0, "profile-level-id=42001f;level-asymmetry-allowed=1", true},
  {"video/AV1", 90000, 0, nullptr},
  {"video/ulpfec", 90000, 0, nullptr},
  {"video/red", 90000, 0, nullptr},
};
inline constexpr RtpExtension kDefaultAudioExtensions[] = {
  {"urn:ietf:params:rtp-hdrext:ssrc-audio-level"},
  {"urn:ietf:params:rtp-hdrext:csrc-audio-level", true},
  {"urn:ietf:params:rtp-hdrext:sdes:mid"},
  {"http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01"},
};
inline constexpr RtpExtension kDefaultVideoExtensions[] = {
  {"urn:ietf:params:rtp-hdrext:sdes:mid"},
  {"http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time"},
  {"urn:ietf:params:rtp-hdrext:toffset"},
  {"http://www.webrtc.org/experiments/rtp-hdrext/playout-delay", true},
  {"http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01"},
  {"urn:3gpp:video-orientation"},
};
inline std::span<const RtpCodec> DefaultCodecs(RtpKind kind) {
  if (kind == RtpKind::Audio) return kDefaultAudioCodecs;
  return kDefaultVideoCodecs;
}
inline std::span<const RtpExtension> DefaultExtensions(RtpKind kind) {
  if (kind == RtpKind::Audio) return kDefaultAudioExtensions;
  return kDefaultVideoExtensions;
}
}
