#ifndef RUYIJS_WEB_RTC_RTP_CODECS_H
#define RUYIJS_WEB_RTC_RTP_CODECS_H
#include <vector>
#include "domain/rtc/rtp_capabilities.h"

namespace ruyijs::web_rtc {
// The codecs getCapabilities(kind) reports for one direction (rtp_capabilities.cpp);
// setCodecPreferences validates against the same list.
std::vector<ruyijs::domain::rtc::RtpCodec> CapabilityCodecs(
    bool audio, ruyijs::domain::rtc::RtpDirection direction);
}  // namespace ruyijs::web_rtc
#endif
