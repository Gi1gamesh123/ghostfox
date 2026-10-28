#ifndef RUYIJS_WEB_AUDIO_REALM_H
#define RUYIJS_WEB_AUDIO_REALM_H
#include <memory>
#include "js/RootingAPI.h"
struct JSContext;
namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::web_audio {
struct State;
// The "audio" profile group (configure-only): sampleRate, baseLatency,
// outputLatency, maxChannelCount. Stored process-wide; see web_audio/profile.h.
bool ConfigureProfileGroup(void* context /*JSContext* */, JS::HandleObject group);
// Brand check for an AudioContext/OfflineAudioContext of any realm, for other
// owners' WebIDL overload resolution.
bool IsAudioContextValue(JS::HandleValue value);
class Realm final {
 public:
  // Reads the Context's single clock owner (execution_scheduling) in seconds.
  // Bound by the shared coordinator once that owner exists; a realm without it
  // (an iframe child, see D8) reports 0 instead of opening a second time source.
  using ClockPort = double (*)(void* context);
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        interaction::Realm& events);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  void SetClockPort(ClockPort clock, void* context);
  // Posts a callback to the Context's one task queue (execution_scheduling):
  // OfflineAudioContext rendering completes in a later task, as in Firefox.
  using TaskPort = bool (*)(void* context, JS::HandleObject callback);
  void SetTaskPort(TaskPort port, void* context);
  // Creates an entangled MessagePort pair through the realm's unique
  // MessagePort owner (web_messaging), bound by the shared coordinator.
  // AudioWorkletNode.port is the first port; the second is the processor-side
  // peer, held by the node until an AudioWorklet execution owner drives it.
  using ChannelPort = bool (*)(void* context, JS::MutableHandleObject first,
                               JS::MutableHandleObject second);
  void SetChannelPort(ChannelPort port, void* context);
 private:
  std::unique_ptr<State> state_;
};
}
#endif
