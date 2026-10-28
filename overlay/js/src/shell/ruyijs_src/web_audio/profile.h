#ifndef RUYIJS_WEB_AUDIO_PROFILE_H
#define RUYIJS_WEB_AUDIO_PROFILE_H
#include <cstdint>
#include "js/RootingAPI.h"
// The "audio" profile group (configure-only). One process-scoped store the
// context constructor and the AudioContext/AudioDestinationNode accessors read,
// so a configure before or after page creation applies to every context
// created afterwards. A field left unset keeps the built-in behaviour.
namespace ruyijs::web_audio {
struct Profile {
  bool hasSampleRate = false, hasBaseLatency = false, hasOutputLatency = false;
  bool hasMaxChannelCount = false;
  double sampleRate = 0, baseLatency = 0, outputLatency = 0;
  int32_t maxChannelCount = 0;
};
Profile CurrentProfile();
// maxChannelCount of a destination: the profile value for a live context, the
// context's own channel count (fallback) for an offline one.
int32_t DestinationMaxChannels(JSObject* context, int32_t fallback);
}
#endif
