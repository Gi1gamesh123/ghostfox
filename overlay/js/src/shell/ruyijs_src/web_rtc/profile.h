#ifndef RUYIJS_WEB_RTC_PROFILE_H
#define RUYIJS_WEB_RTC_PROFILE_H
// The "rtc" profile group's values, owned by web_rtc. Process-scoped like the
// canvas fingerprint configuration: one Context is one runtime process. A field
// the group leaves out stays unset and keeps the recorded Firefox behaviour.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ruyijs::web_rtc {
struct ProfileCodec {
  std::string mime;
  uint32_t clock = 0;
  uint16_t channels = 0;
  std::string fmtp;
  bool receiveOnly = false;
};
struct Profile {
  std::optional<std::vector<ProfileCodec>> audioCodecs, videoCodecs;
  std::optional<std::string> sessionId, iceUfrag, icePwd, dtlsFingerprint;
  std::vector<std::string> iceCandidates;
};
const Profile& CurrentProfile();
void SetProfile(Profile profile);
}
#endif
