#ifndef RUYIJS_WEB_MIDI_REALM_H
#define RUYIJS_WEB_MIDI_REALM_H
#include <memory>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::web_midi {
struct State;
// Web MIDI for one Window realm (docs/owners/WEB_MIDI.md): the eight MIDI
// interface objects, the two constructible events and
// Navigator.requestMIDIAccess. There are no MIDI devices and no permission
// prompt; like Firefox waiting on its prompt, the access promise stays pending,
// so MIDIAccess/MIDIPort instances are never created.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events,
        browser_environment::Environment& environment);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  // [SecureContext] requestMIDIAccess and every MIDI interface.
  bool ConfigureExposure();
 private:
  std::unique_ptr<State> state_;
};
}
#endif
