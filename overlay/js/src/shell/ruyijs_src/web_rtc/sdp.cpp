#include "web_rtc/internal.h"

#include "domain/rtc/dtls_certificate.h"
#include "domain/rtc/sdp_media.h"
#include "web_rtc/profile.h"
#include "mozilla/RandomNum.h"
#include <vector>

namespace ruyijs::web_rtc {
namespace {

namespace rtc = ruyijs::domain::rtc;

// The imported aggregate's placeholder offer; only that shape is replaced.
const char* const kTemplateSdp = "v=0\r\n";

std::string Hex(const uint8_t* bytes, size_t size) {
  static const char kDigits[] = "0123456789abcdef";
  std::string out;
  out.reserve(size * 2);
  for (size_t i = 0; i < size; ++i) {
    out.push_back(kDigits[bytes[i] >> 4]);
    out.push_back(kDigits[bytes[i] & 0x0f]);
  }
  return out;
}

// 19 decimal digits with a non-zero leading digit, the width Firefox's
// THIS_IS_SDPARTA session id has.
std::string SessionId(const uint8_t* random) {
  uint64_t value = 0;
  for (int i = 0; i < 8; ++i) value = (value << 8) | random[i];
  value = 1000000000000000000ull + value % 9000000000000000000ull;
  return std::to_string(value);
}

}  // namespace

// An empty peer announces session-level ICE/DTLS attributes but no m-section;
// one or more data channels add the shared SCTP m-section. Media-bearing peers
// keep the aggregate's fixed template until their media SDP is modeled.
bool FillOffer(State& state, uint32_t peer, DescriptionSpec* spec) {
  std::vector<uint32_t> channels, transceivers;
  if (!state.model.peer_channels(peer, &channels) ||
      !state.model.peer_transceivers(peer, &transceivers))
    return true;
  if (spec->sdp != kTemplateSdp) return true;
  uint8_t random[8 + rtc::kDtlsFingerprintBytes + 4 + 16 + 16];
  if (!mozilla::GenerateRandomBytesFromOS(random, sizeof(random))) return false;
  const uint8_t* cursor = random;
  rtc::DataChannelOfferInput input;
  input.session_id = SessionId(cursor);
  cursor += 8;
  input.fingerprint = rtc::FormatFingerprint(cursor, rtc::kDtlsFingerprintBytes);
  cursor += rtc::kDtlsFingerprintBytes;
  input.ice_ufrag = Hex(cursor, 4);
  cursor += 4;
  input.ice_pwd = Hex(cursor, 16);
  cursor += 16;
  const Profile& profile = CurrentProfile();
  if (profile.sessionId) input.session_id = *profile.sessionId;
  if (profile.dtlsFingerprint) input.fingerprint = *profile.dtlsFingerprint;
  if (profile.iceUfrag) input.ice_ufrag = *profile.iceUfrag;
  if (profile.icePwd) input.ice_pwd = *profile.icePwd;
  if (transceivers.empty()) {
    spec->sdp = channels.empty() ? rtc::EmptyOffer(input) : rtc::DataChannelOffer(input);
    return true;
  }
  // One CNAME per offer, Firefox's braced UUID; a random SSRC per media section.
  const std::string id = Hex(cursor, 16);
  const std::string cname = "{" + id.substr(0, 8) + "-" + id.substr(8, 4) + "-4" + id.substr(13, 3) + "-" +
                            "89ab"[cursor[8] & 3] + id.substr(17, 3) + "-" + id.substr(20, 12) + "}";
  std::vector<rtc::MediaSectionInput> media;
  for (uint32_t transceiver : transceivers) {
    TransceiverInfo info;
    uint32_t ssrc = 0;
    if (!state.model.transceiver_info(transceiver, &info) ||
        !mozilla::GenerateRandomBytesFromOS(&ssrc, sizeof ssrc)) return false;
    media.push_back({info.kind, info.direction, ssrc});
  }
  spec->sdp = rtc::MediaOffer(input, media, !channels.empty(), cname);
  return true;
}

}  // namespace ruyijs::web_rtc
