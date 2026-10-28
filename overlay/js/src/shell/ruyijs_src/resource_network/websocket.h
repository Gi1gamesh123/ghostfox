#ifndef RUYIJS_RESOURCE_WEBSOCKET_H
#define RUYIJS_RESOURCE_WEBSOCKET_H
#include "resource_network/state.h"
namespace ruyijs::resource_network {
enum WebSocketSlot : int32_t { WUrl = 0, WReadyState, WOnError, WSlotCount };
bool WebSocketConstructor(JSContext*, unsigned, JS::Value*);
bool WebSocketGet(JSContext*, unsigned, JS::Value*);
bool WebSocketSet(JSContext*, unsigned, JS::Value*);
bool WebSocketClose(JSContext*, unsigned, JS::Value*);
}
#endif
