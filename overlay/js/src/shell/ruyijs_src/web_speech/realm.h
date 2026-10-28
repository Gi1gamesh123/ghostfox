#ifndef RUYIJS_WEB_SPEECH_REALM_H
#define RUYIJS_WEB_SPEECH_REALM_H
#include <memory>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::web_speech {
struct State;
// The Speech Synthesis owner of one Window realm (docs/owners/SPEECH_SYNTHESIS.md).
// The main context owns the one VoiceList; iframe and successor realms share it
// through `origin`. Events go through the Interaction owner's one factory.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        interaction::Realm& events, Realm* origin);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  // Installs SpeechSynthesis, SpeechSynthesisUtterance, SpeechSynthesisVoice and
  // the Window's own `speechSynthesis` accessor.
  bool Install();
  // Browser Environment "speech" group port, already validated.
  bool Configure(JS::HandleObject group);
  // Scheduling port: queues one speaking step on the Context's macrotask queue
  // after `delay_ms` (speech takes time, so cancel() can reach a running utterance).
  using TaskPort = bool (*)(void* context, JS::HandleObject task, double delay_ms);
  void SetTaskPort(TaskPort port, void* context);
  // A frame that commits a document of another site than its top-level one
  // runs in its own content process under Fission, where the platform voices
  // are enumerated again: this realm takes its own copy of the list, empty
  // until its own publication task fires `voiceschanged` (Firefox).
  void IsolateVoices();
 private:
  void Release();
  std::unique_ptr<State> state_;
};
}
#endif
