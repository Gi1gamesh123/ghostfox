// The "audio" profile group: validation (Firefox ranges) and the process-scoped
// store behind web_audio/profile.h.
#include "web_audio/profile.h"
#include <cmath>
#include <mutex>
#include <string>
#include "jsapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "runtime/web_kit.h"
#include "web_audio/realm.h"
#include "web_audio/state.h"

namespace ruyijs::web_audio {
namespace {
std::mutex gLock;
Profile gProfile;

bool Read(JSContext* cx, JS::HandleObject group, const char* name, double low,
          double high, bool integer, bool* present, double* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, group, name, &value)) return false;
  *present = !value.isUndefined();
  if (!*present) return true;
  const double v = value.isNumber() ? value.toNumber() : NAN;
  if (!std::isfinite(v) || v < low || v > high || (integer && v != std::floor(v)))
    return WebKit::Fail(cx, (std::string("audio.") + name + " is out of range").c_str());
  *out = v;
  return true;
}
}  // namespace

Profile CurrentProfile() {
  std::lock_guard<std::mutex> hold(gLock);
  return gProfile;
}

int32_t DestinationMaxChannels(JSObject* context, int32_t fallback) {
  const Profile p = CurrentProfile();
  const bool offline = context && JS::GetReservedSlot(context, kContextOfflineSlot).toBoolean();
  return (p.hasMaxChannelCount && !offline) ? p.maxChannelCount : fallback;
}

bool ConfigureProfileGroup(void* context, JS::HandleObject group) {
  JSContext* cx = static_cast<JSContext*>(context);
  Profile next;
  // Firefox: AudioContext sampleRate is within [3000, 768000] (WebAudioUtils).
  if (!Read(cx, group, "sampleRate", 3000, 768000, false, &next.hasSampleRate, &next.sampleRate) ||
      !Read(cx, group, "baseLatency", 0, 10, false, &next.hasBaseLatency, &next.baseLatency) ||
      !Read(cx, group, "outputLatency", 0, 10, false, &next.hasOutputLatency, &next.outputLatency))
    return false;
  double channels = 0;
  if (!Read(cx, group, "maxChannelCount", 1, kMaxChannelCount, true, &next.hasMaxChannelCount,
            &channels)) return false;
  next.maxChannelCount = static_cast<int32_t>(channels);
  std::lock_guard<std::mutex> hold(gLock);
  gProfile = next;
  return true;
}
}  // namespace ruyijs::web_audio
