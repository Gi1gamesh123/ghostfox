// EventSource (docs/owners/RESOURCE_NETWORK.md): instance slots and the connection
// steps shared by event_source.cpp (connection) and event_source_install.cpp (interface).
#ifndef RUYIJS_RESOURCE_EVENT_SOURCE_H
#define RUYIJS_RESOURCE_EVENT_SOURCE_H
#include "resource_network/state.h"
namespace ruyijs::resource_network {
enum EventSourceSlot : uint32_t {
  EUrl, ECredentials, EState, EOnOpen, EOnMessage, EOnError, ELastId, ERetry, EGeneration, EOwner, ESlots
};
extern const JSClass kEventSourceClass;
// Opens (or re-opens) the connection of `self`; readyState is CONNECTING.
bool EventSourceConnect(State&, JS::HandleObject self);
}  // namespace ruyijs::resource_network
#endif
