#include "resource_network/websocket.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::resource_network {
bool InstallWebSocket(State& state, PageHelpers& helpers) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject eventTarget(cx, state.events ? state.events->prototype() : nullptr);
  if (!eventTarget || !state.kit.Define("WebSocket", &kWebSocketClass, eventTarget,
                                         WebSocketConstructor, &state.websocket, 1)) return false;
  if (!state.kit.Method(state.websocket, "WebSocket", "close", WebSocketClose, 0, 0) ||
      !state.kit.Accessor(state.websocket, "WebSocket", "onerror", WebSocketGet,
                          WebSocketSet, WOnError)) return false;
  const char* names[] = {"CONNECTING", "OPEN", "CLOSING", "CLOSED"};
  for (int i = 0; i < 4; ++i) {
    const unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
    if (!JS_DefineProperty(cx, state.websocket.constructor, names[i], i, flags) ||
        !JS_DefineProperty(cx, state.websocket.prototype, names[i], i, flags)) return false;
  }
  return true;
}
}
