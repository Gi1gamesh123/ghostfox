// Service Worker events (dom/webidl/ExtendableEvent.webidl, ExtendableMessageEvent.webidl,
// FetchEvent.webidl), exposed only in a ServiceWorkerGlobalScope. The Service Worker owner
// fires them trusted (Realm::FireExtendable) and reads the waitUntil promises the listeners
// added; lifetimes, clients and registrations stay in that owner.
#include "interaction/internal.h"
#include "interaction/event_creation.h"
#include "interaction/realm.h"
#include "interaction/runtime_event.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/ValueArray.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::interaction {
namespace {
JSObject* Self(const JS::CallArgs& args) { return &args.thisv().toObject(); }
State* StateOf(JSContext* cx) {
  State* state = activeState;
  while (state && state->page != JS::CurrentGlobalOrNull(cx)) state = state->previous;
  return state;
}
bool InvalidState(JSContext* cx, const char* message) {
  State* state = StateOf(cx);
  if (state && state->exceptionReporter)
    return state->exceptionReporter(state->exceptionContext, "InvalidStateError", message);
  // Worker realms have no reporter port: the realm's own DOMException.
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  JS::RootedValue constructor(cx), error(cx);
  JS::RootedObject created(cx);
  JS::RootedValueArray<2> values(cx);
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message)), name(cx, JS_NewStringCopyZ(cx, "InvalidStateError"));
  if (!global || !text || !name || !JS_GetProperty(cx, global, "DOMException", &constructor) || !constructor.isObject())
    return Error(cx, "TypeError", message);
  values[0].setString(text);
  values[1].setString(name);
  if (!JS::Construct(cx, constructor, values, &created)) return false;
  error.setObject(*created);
  JS_SetPendingException(cx, error);
  return false;
}
// Firefox ExtendableEvent::WaitUntil: only a dispatched trusted event extends a lifetime.
bool WaitUntil(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Extendable);
  if (!event) return false;
  ReportEventAccess(cx, event, "ExtendableEvent", "waitUntil", int(ApiAccess::Call));
  if (!argc) return Error(cx, "TypeError", "ExtendableEvent.waitUntil: At least 1 argument required, but only 0 passed");
  JS::RootedObject promise(cx, JS::CallOriginalPromiseResolve(cx, args[0]));
  if (!promise) return false;
  JS::RootedValue list(cx, JS::GetReservedSlot(Self(args), kEventLifetimes));
  if (!list.isObject() || !event->value.dispatching())
    return InvalidState(cx, "An attempt was made to use an object that is not, or is no longer, usable");
  JS::RootedObject array(cx, &list.toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, array, &length) || !JS_SetElement(cx, array, length, promise)) return false;
  args.rval().setUndefined();
  return true;
}
#define MESSAGE_GETTER(name, member, slot)                                              \
  bool name(JSContext* cx, unsigned argc, JS::Value* vp) {                              \
    auto args = JS::CallArgsFromVp(argc, vp);                                           \
    Event* event = Receiver(cx, args, Iface::ExtendableMessage);                        \
    if (!event) return false;                                                           \
    ReportEventAccess(cx, event, "ExtendableMessageEvent", member, int(ApiAccess::Read)); \
    args.rval().set(JS::GetReservedSlot(Self(args), slot));                             \
    return JS_WrapValue(cx, args.rval());                                               \
  }
MESSAGE_GETTER(Data, "data", kEventMessageData)
MESSAGE_GETTER(Origin, "origin", kEventOrigin)
MESSAGE_GETTER(LastEventId, "lastEventId", kEventLastEventId)
MESSAGE_GETTER(Source, "source", kEventMessageSource)
MESSAGE_GETTER(Ports, "ports", kEventPorts)
#define FETCH_GETTER(name, member, slot)                                               \
  bool name(JSContext* cx, unsigned argc, JS::Value* vp) {                             \
    auto args = JS::CallArgsFromVp(argc, vp);                                          \
    Event* event = Receiver(cx, args, Iface::Fetch);                                   \
    if (!event) return false;                                                          \
    ReportEventAccess(cx, event, "FetchEvent", member, int(ApiAccess::Read));          \
    args.rval().set(JS::GetReservedSlot(Self(args), slot));                            \
    return JS_WrapValue(cx, args.rval());                                              \
  }
FETCH_GETTER(Request, "request", kEventPublicFirst)
FETCH_GETTER(PreloadResponse, "preloadResponse", kEventMessageData)
FETCH_GETTER(ClientId, "clientId", kEventPublicSecond)
FETCH_GETTER(ResultingClientId, "resultingClientId", kEventPublicThird)
FETCH_GETTER(Handled, "handled", kEventPublicFourth)
// No fetch is ever routed to a service worker here, so only a dispatched
// trusted FetchEvent could accept a response; every other call is Firefox's
// InvalidStateError.
bool RespondWith(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Fetch);
  if (!event) return false;
  ReportEventAccess(cx, event, "FetchEvent", "respondWith", int(ApiAccess::Call));
  if (!argc) return Error(cx, "TypeError", "FetchEvent.respondWith: At least 1 argument required, but only 0 passed");
  return InvalidState(cx, "An attempt was made to use an object that is not, or is no longer, usable");
}
bool ResolvedPromise(JSContext* cx, JS::MutableHandleValue out) {
  JSObject* promise = JS::CallOriginalPromiseResolve(cx, JS::UndefinedHandleValue);
  if (!promise) return false;
  out.setObject(*promise);
  return true;
}
}  // namespace

extern const JSFunctionSpec kExtendableMethods[] = {
  JS_FN("waitUntil", WaitUntil, 1, JSPROP_ENUMERATE), JS_FS_END};
extern const JSPropertySpec kExtendableMessageProperties[] = {
  JS_PSG("data", Data, JSPROP_ENUMERATE), JS_PSG("origin", Origin, JSPROP_ENUMERATE),
  JS_PSG("lastEventId", LastEventId, JSPROP_ENUMERATE), JS_PSG("source", Source, JSPROP_ENUMERATE),
  JS_PSG("ports", Ports, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kFetchProperties[] = {
  JS_PSG("request", Request, JSPROP_ENUMERATE), JS_PSG("preloadResponse", PreloadResponse, JSPROP_ENUMERATE),
  JS_PSG("clientId", ClientId, JSPROP_ENUMERATE), JS_PSG("resultingClientId", ResultingClientId, JSPROP_ENUMERATE),
  JS_PSG("handled", Handled, JSPROP_ENUMERATE), JS_PS_END};
extern const JSFunctionSpec kFetchMethods[] = {
  JS_FN("respondWith", RespondWith, 1, JSPROP_ENUMERATE), JS_FS_END};

bool IsServiceWorkerEvent(Iface iface) {
  return iface == Iface::Extendable || iface == Iface::ExtendableMessage || iface == Iface::Fetch;
}
bool InitFetch(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  JS::RootedValue request(cx);
  if (dict && !ReadInitValue(cx, dict, "request", &request)) return false;
  if (!request.isObject())
    return Error(cx, "TypeError", request.isUndefined()
        ? "FetchEvent constructor: Missing required 'request' member of FetchEventInit."
        : "FetchEvent constructor: 'request' member of FetchEventInit is not an object.");
  std::string client, resulting;
  JS::RootedValue preload(cx), handled(cx);
  if (!ReadInitString(cx, dict, "clientId", &client) ||
      !ReadInitString(cx, dict, "resultingClientId", &resulting) ||
      !ResolvedPromise(cx, &preload) || !ResolvedPromise(cx, &handled)) return false;
  JS::SetReservedSlot(object, kEventPublicFirst, request);
  JS::SetReservedSlot(object, kEventMessageData, preload);
  JS::SetReservedSlot(object, kEventPublicFourth, handled);
  return SetSlotText(cx, object, kEventPublicSecond, client) &&
         SetSlotText(cx, object, kEventPublicThird, resulting);
}

bool Realm::FireExtendable(JS::HandleObject target, const char* type, JS::HandleObject init,
                           bool message, JS::MutableHandleObject lifetimes) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  const Iface iface = message ? Iface::ExtendableMessage : Iface::Extendable;
  JS::RootedObject created(cx);
  if (!FireRuntimeEvent(state, target, message ? "ExtendableMessageEvent" : "ExtendableEvent", type, iface,
                        message ? Kind::ExtendableMessage : Kind::Extendable, init, nullptr, -1, true,
                        nullptr, false, &created)) return false;
  JS::RootedValue list(cx, created ? JS::GetReservedSlot(created, kEventLifetimes) : JS::UndefinedValue());
  lifetimes.set(list.isObject() ? &list.toObject() : nullptr);
  return true;
}
}  // namespace ruyijs::interaction
