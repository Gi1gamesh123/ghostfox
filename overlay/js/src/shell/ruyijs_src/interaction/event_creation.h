#ifndef RUYIJS_INTERACTION_EVENT_CREATION_H
#define RUYIJS_INTERACTION_EVENT_CREATION_H
#include <cstdint>
#include "js/RootingAPI.h"
namespace ruyijs::interaction {
struct State;
enum class Iface : uint8_t;
// Cache only references to prototypes built by their existing installers.
bool SaveLegacyPrototype(State&, Iface, JS::HandleObject);
bool ReadInstalledEventPrototype(State&, Iface, JS::MutableHandleObject);
}
#endif
