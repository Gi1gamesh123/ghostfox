#include "web_media/media_session.h"
#include "web_media/media_runtime.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_media {
namespace {
enum SessionSlot { SessionMetadata, SessionPlayback, SessionHandlers, SessionOwner, SessionSlots };
const JSClass kSessionClass = {"MediaSession", JSCLASS_HAS_RESERVED_SLOTS(SessionSlots)};

bool Receiver(JSContext* cx, JS::HandleValue value, JS::MutableHandleObject out) {
  return MediaReceiver(cx, value, &kSessionClass, out);
}

bool SessionGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &self)) return false;
  JS::RootedValue value(cx, JS::GetReservedSlot(self, WebKit::Slot(args)));
  if (!JS_WrapValue(cx, &value)) return false;
  args.rval().set(value);
  return true;
}

bool SessionSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &self) || !argc) return false;
  const int slot = WebKit::Slot(args);
  JS::RootedValue value(cx, args[0]);
  if (slot == SessionMetadata) {
    if (!value.isNull() && (!value.isObject() || !IsMediaMetadataObject(&value.toObject())))
      return WebKit::Fail(cx, "MediaSession metadata must be a MediaMetadata or null");
  } else {
    JS::RootedString text(cx, JS::ToString(cx, value));
    if (!text) return false;
    bool none = false, paused = false, playing = false;
    if (!JS_StringEqualsAscii(cx, text, "none", &none) ||
        !JS_StringEqualsAscii(cx, text, "paused", &paused) ||
        !JS_StringEqualsAscii(cx, text, "playing", &playing) || !(none || paused || playing))
      return WebKit::Fail(cx, "Invalid MediaSession playbackState");
    value.setString(text);
  }
  JS::SetReservedSlot(self, slot, value);
  args.rval().setUndefined();
  return true;
}

bool SessionMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &self)) return false;
  const int op = WebKit::Slot(args);
  if (op == 0) {
    if (argc < 1) return WebKit::Fail(cx, "setActionHandler requires an action");
    JS::RootedString action(cx, JS::ToString(cx, args[0]));
    if (!action) return false;
    const char* names[] = {"play", "pause", "seekbackward", "seekforward", "previoustrack",
                           "nexttrack", "skipad", "seekto", "stop"};
    bool valid = false;
    for (const char* name : names) {
      bool equal = false;
      if (!JS_StringEqualsAscii(cx, action, name, &equal)) return false;
      valid |= equal;
    }
    if (!valid) return WebKit::Fail(cx, "Invalid MediaSession action");
    JS::RootedValue handler(cx, argc > 1 ? args[1] : JS::NullValue());
    if (!handler.isNull() && !handler.isUndefined() &&
        (!handler.isObject() || !JS::IsCallable(&handler.toObject())))
      return WebKit::Fail(cx, "MediaSession action handler must be callable or null");
    JSAutoRealm realm(cx, self);
    JS::RootedValue handlers(cx, JS::GetReservedSlot(self, SessionHandlers));
    JS::RootedString key(cx, action);
    JS::RootedObject handlerMap(cx, handlers.isObject() ? &handlers.toObject() : nullptr);
    JS::RootedId id(cx);
    if (!handlerMap || !JS_StringToId(cx, key, &id) || !JS_SetPropertyById(cx, handlerMap, id, handler)) return false;
  } else if (argc && !args[0].isUndefined() && !args[0].isNull() && !args[0].isObject()) {
    return WebKit::Fail(cx, "MediaSession position state must be an object");
  }
  args.rval().setUndefined();
  return true;
}

bool NavigatorSession(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  if (!args.thisv().isObject() || &args.thisv().toObject() != state.navigator)
    return WebKit::Fail(cx, "Navigator.mediaSession called on an incompatible receiver");
  args.rval().setObject(*state.mediaSession);
  return true;
}

bool SessionConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  return WebKit::Fail(cx, "Illegal constructor.");
}
}

bool InstallMediaSessionObject(State& state) {
  JS::RootedObject parent(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!state.kit.Define("MediaSession", &kSessionClass, parent, SessionConstructor,
                        &state.session) ||
      !state.kit.Method(state.session, "MediaSession", "setActionHandler", SessionMethod, 2, 0) ||
      !state.kit.Method(state.session, "MediaSession", "setPositionState", SessionMethod, 0, 1) ||
      !state.kit.Accessor(state.session, "MediaSession", "metadata", SessionGet, SessionSet, SessionMetadata) ||
      !state.kit.Accessor(state.session, "MediaSession", "playbackState", SessionGet, SessionSet, SessionPlayback) ||
      !MediaConstructorLast(state, state.session)) return false;
  state.mediaSession = state.kit.New(&kSessionClass, state.session.prototype);
  if (!state.mediaSession) return false;
  JS::RootedString none(state.cx, JS_NewStringCopyZ(state.cx, "none"));
  JS::RootedObject handlers(state.cx, JS_NewPlainObject(state.cx));
  if (!none || !handlers) return false;
  JS::SetReservedSlot(state.mediaSession, SessionMetadata, JS::NullValue());
  JS::SetReservedSlot(state.mediaSession, SessionPlayback, JS::StringValue(none));
  JS::SetReservedSlot(state.mediaSession, SessionHandlers, JS::ObjectValue(*handlers));
  JS::SetReservedSlot(state.mediaSession, SessionOwner, JS::PrivateValue(&state));
  JS::RootedObject proto(state.cx, state.environment.navigatorPrototype());
  JS::RootedObject getter(state.cx, state.kit.Function(NavigatorSession, "get mediaSession", 0, 0));
  JS::RootedObject setter(state.cx);
  return getter && JS_DefineProperty(state.cx, proto, "mediaSession", getter, setter, JSPROP_ENUMERATE) &&
         state.kit.helpers->Accessor("Navigator", proto, "mediaSession");
}
}
