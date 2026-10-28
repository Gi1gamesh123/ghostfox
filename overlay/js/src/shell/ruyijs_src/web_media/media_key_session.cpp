#include "web_media/media_key_internal.h"
#include "web_media/media_runtime.h"
#include "runtime/page_helpers.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "jsapi.h"

namespace ruyijs::web_media {
const JSClass kMediaKeySessionClass = {"MediaKeySession", JSCLASS_HAS_RESERVED_SLOTS(SessionSlotCount)};

namespace {
bool Receiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject out) {
  return MediaReceiver(cx, args.thisv(), &kMediaKeySessionClass, out);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false;
  return MediaGetSlot(cx, self, WebKit::Slot(args), args.rval());
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false;
  auto& state = *static_cast<State*>(WebKit::Owner(args)); const int slot = WebKit::Slot(args);
  const char* type = slot == SessionStatusHandler ? "keystatuseschange" : "message";
  if (!MediaHandlerSet(state, self, slot, type, args.get(0))) return false;
  args.rval().setUndefined(); return true;
}
bool Method(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false;
  const int op = WebKit::Slot(args);
  if (op == 3 || op == 4) {
    if (!JS::GetReservedSlot(self, SessionClosed).toBoolean()) {
      JS::RootedValue closed(cx, JS::GetReservedSlot(self, SessionOwner));
      JS::SetReservedSlot(self, SessionClosed, JS::BooleanValue(true));
      if (closed.isObject()) {
        JS::RootedObject promise(cx, &closed.toObject());
        if (!JS::ResolvePromise(cx, promise, JS::UndefinedHandleValue)) return false;
      }
    }
  }
  if (op == 1) {
    JS::RootedValue falseValue(cx, JS::BooleanValue(false));
    return PromiseValue(cx, falseValue, args.rval());
  }
  return PromiseUndefined(cx, args.rval());
}
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) { return WebKit::Reject(cx, argc, vp); }
}

bool NewMediaKeySession(State& state, JS::MutableHandleObject out) {
  JS::RootedObject object(state.cx, state.kit.New(&kMediaKeySessionClass, state.keySession.prototype));
  JS::RootedString id(state.cx, JS_NewStringCopyZ(state.cx, ""));
  JS::RootedObject closed(state.cx, JS::NewPromiseObject(state.cx, nullptr));
  JS::RootedObject statuses(state.cx, state.kit.New(&kMediaKeyStatusMapClass, state.keyStatusMap.prototype));
  if (!object || !id || !closed || !statuses || !state.events.Attach(object)) return false;
  JS::SetReservedSlot(object, SessionId, JS::StringValue(id));
  JS::SetReservedSlot(object, SessionExpiration, JS::DoubleValue(std::numeric_limits<double>::quiet_NaN()));
  JS::SetReservedSlot(object, SessionClosed, JS::BooleanValue(false));
  JS::SetReservedSlot(object, SessionStatuses, JS::ObjectValue(*statuses));
  JS::SetReservedSlot(object, SessionError, JS::NullValue());
  JS::SetReservedSlot(object, SessionStatusHandler, JS::NullValue());
  JS::SetReservedSlot(object, SessionMessageHandler, JS::NullValue());
  JS::SetReservedSlot(object, SessionOwner, JS::ObjectValue(*closed));
  out.set(object); return true;
}

bool InstallMediaKeySession(State& state) {
  JS::RootedObject parent(state.cx, state.events.prototype());
  if (!state.kit.Define("MediaKeySession", &kMediaKeySessionClass, parent, Illegal, &state.keySession)) return false;
  const char* methods[] = {"generateRequest", "load", "update", "close", "remove"};
  const unsigned lengths[] = {2, 1, 1, 0, 0};
  for (unsigned i = 0; i < 5; ++i)
    if (!state.kit.Method(state.keySession, "MediaKeySession", methods[i], Method, lengths[i], int32_t(i))) return false;
  const char* fields[] = {"error", "sessionId", "expiration", "closed", "keyStatuses"};
  const int slots[] = {SessionError, SessionId, SessionExpiration, SessionOwner, SessionStatuses};
  for (unsigned i = 0; i < 5; ++i)
    if (!state.kit.Accessor(state.keySession, "MediaKeySession", fields[i], Get, nullptr, slots[i])) return false;
  return state.kit.Accessor(state.keySession, "MediaKeySession", "onkeystatuseschange", Get, Set, SessionStatusHandler) &&
         state.kit.Accessor(state.keySession, "MediaKeySession", "onmessage", Get, Set, SessionMessageHandler) &&
         MediaConstructorLast(state, state.keySession);
}
}
