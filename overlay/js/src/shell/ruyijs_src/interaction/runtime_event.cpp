// Runtime-originated trusted events. The runtime fires these without a script
// constructor call, so the object is built from the installed interface
// prototype and the same traced slot tables, then delivered on the target's
// resolved event path. Window.postMessage (MessageEvent), History traversal
// (PopStateEvent), the Location fragment setter (HashChangeEvent) and the
// OfflineAudioContext render completion share this one construction path.
#include "interaction/internal.h"
#include "interaction/close_watcher.h"
#include "interaction/event_creation.h"
#include "interaction/runtime_event.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Wrapper.h"
#include <cstring>
#include <string>

namespace ruyijs::interaction {
namespace {
int gHandlingUserInput = 0;
bool UserInteraction(const char* type) {
  for (const char* name : {"mousedown", "mouseup", "pointerdown", "pointerup", "click", "auxclick", "dblclick",
                           "contextmenu", "keydown", "keyup", "keypress", "touchstart", "touchend"})
    if (!std::strcmp(type, name)) return true;
  return false;
}
}  // namespace
bool HandlingUserInput() { return gHandlingUserInput > 0; }
bool FireRuntimeEvent(State& state, JS::HandleObject target, const char* interface_name,
                      const char* type, Iface iface, Kind kind, JS::HandleObject init,
                      JS::HandleObject installed, double time_ms, bool trusted, bool* allowed, bool hostInput,
                      JS::RootedObject* created) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  if (state.events.size() >= 100000) return Error(cx, "RangeError", "Event limit exceeded");
  JS::RootedValue constructor(cx);
  JS::RootedValue prototype(cx, installed ? JS::ObjectValue(*installed) : JS::UndefinedValue());
  if (!installed && (!JS_GetProperty(cx, state.page, interface_name, &constructor) || !constructor.isObject()))
    return Error(cx, "TypeError", (std::string(interface_name) +
                                   " interface is not installed").c_str());
  JS::RootedObject interface(cx, constructor.isObject() ? &constructor.toObject() : nullptr);
  if (!installed && (!JS_GetProperty(cx, interface, "prototype", &prototype) || !prototype.isObject()))
    return Error(cx, "TypeError", (std::string(interface_name) +
                                   " prototype is not installed").c_str());
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, type));
  JS::RootedString text(cx);
  JS::RootedValue name_value(cx, name ? JS::StringValue(name) : JS::UndefinedValue());
  std::string key;
  if (!name || !Text(cx, name_value, &text, &key)) return false;
  JS::RootedObject event_prototype(cx, &prototype.toObject());
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kEventClass, event_prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, kEventKind, JS::Int32Value(static_cast<int32_t>(kind)));
  JS::RootedValue dictionary(cx, JS::ObjectValue(*init));
  if (!InitEventFields(cx, iface, dictionary, object) || !InstallTrusted(state, object))
    return false;
  // A trusted ExtendableEvent collects waitUntil promises for the firing owner.
  if (trusted && IsA(kind, Iface::Extendable)) {
    JS::RootedObject lifetimes(cx, JS::NewArrayObject(cx, 0));
    if (!lifetimes) return false;
    JS::SetReservedSlot(object, kEventLifetimes, JS::ObjectValue(*lifetimes));
  }
  if (created) *created = object;
  // Native form submission carries the original FormData, unlike the legacy
  // public ruyijs placeholder constructor. Both getters read this same traced slot.
  if (iface == Iface::FormDataPayload) {
    JS::RootedValue data(cx);
    if (!JS_GetProperty(cx, init, "formData", &data)) return false;
    JS::SetReservedSlot(object, kEventPublicFirst, data);
  }
  // A runtime speech event keeps its payload for the speech getters.
  if (iface == Iface::Speech || iface == Iface::SpeechError)
    JS::SetReservedSlot(object, kEventPublicSecond, JS::ObjectValue(*init));
  bool bubbles = false, cancelable = false, composed = false;
  if (!ReadEventBase(cx, dictionary, &bubbles, &cancelable, &composed)) return false;
  auto event = std::make_unique<Event>(cx, object, text, std::move(key), bubbles, cancelable);
  event->kind = kind;
  event->value.set_composed(composed);
  if (trusted) event->value.mark_trusted();
  event->value.set_time_stamp(time_ms >= 0 ? time_ms : std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - state.started).count());
  Event& entry = *event;
  state.events.push_back(std::move(event));
  PreparedPath path(cx);
  if (!PreparePath(state, target, entry, &path)) return false;
  if (hostInput && trusted && !NotifyInputActivation(state, target, object, type)) return false;
  const bool userInput = hostInput && trusted && UserInteraction(type);
  gHandlingUserInput += userInput;
  bool result = Deliver(entry, path);
  gHandlingUserInput -= userInput;
  if (allowed) *allowed = result;
  return !hostInput || !trusted || ProcessCloseInput(state,target,object,type,result);
}

// The `input` RPC: one trusted Mouse/Pointer event through the shared
// trusted-input factory (trusted_input.cpp).
bool DispatchInput(JSContext* cx, JS::HandleObject target, const char* type,
                   JS::HandleObject init, double time_ms) {
  bool accepted = false;
  for (const char* name : {"pointerdown", "pointermove", "pointerup", "pointercancel",
                          "pointerover", "pointerout", "pointerenter", "pointerleave",
                          "mousedown", "mousemove", "mouseup", "click", "dblclick",
                          "contextmenu", "mouseover", "mouseout", "mouseenter", "mouseleave"})
    if (!std::strcmp(type, name)) accepted = true;
  if (!accepted) return Error(cx, "TypeError", "Unsupported Mouse/Pointer input type");
  return DispatchTrusted(cx, target, type, init, time_ms, nullptr);
}

// The Window facade hands the already-cloned message here; delivery is the same
bool Realm::FireTaskPriority(JS::HandleObject target,JS::HandleObject prototype,const char* previous,double time_ms){
 State& s=*state_;JSAutoRealm realm(s.cx,s.page);JS::RootedObject init(s.cx,JS_NewPlainObject(s.cx));
 JS::RootedString text(s.cx,JS_NewStringCopyZ(s.cx,previous));
 return init&&text&&JS_DefineProperty(s.cx,init,"previousPriority",text,JSPROP_ENUMERATE)&&
   FireRuntimeEvent(s,target,"TaskPriorityChangeEvent","prioritychange",Iface::TaskPriority,Kind::TaskPriority,init,prototype,time_ms);
}

bool Realm::FireError(JS::HandleObject target, JS::HandleValue error,
                     const char* message, const char* filename,
                     int32_t line, int32_t column, bool* allowed) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject init(cx, JS_NewPlainObject(cx));
  JS::RootedString messageValue(cx, JS_NewStringCopyZ(cx, message ? message : ""));
  JS::RootedString filenameValue(cx, JS_NewStringCopyZ(cx, filename ? filename : ""));
  JS::RootedValue errorValue(cx, error);
  if (!init || !messageValue || !filenameValue || !JS_WrapValue(cx, &errorValue) ||
      !JS_DefineProperty(cx, init, "message", messageValue, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "filename", filenameValue, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "lineno", line, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "colno", column, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "error", errorValue, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "bubbles", false, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "cancelable", true, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "composed", false, JSPROP_ENUMERATE)) return false;
  return FireRuntimeEvent(state, target, "ErrorEvent", "error", Iface::Error,
                          Kind::Error, init, nullptr, -1, true, allowed);
}

// A cookie jar write reports what changed; the Resource Network owner builds the
// two lists and this fires the one trusted CookieChangeEvent at the CookieStore.
bool Realm::FireCookieChange(JS::HandleObject target, JS::HandleValue changed,
                             JS::HandleValue deleted) {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.page);
  JS::RootedObject init(cx, JS_NewPlainObject(cx));
  JS::RootedValue changedValue(cx, changed), deletedValue(cx, deleted);
  if (!init || !JS_WrapValue(cx, &changedValue) || !JS_WrapValue(cx, &deletedValue)) return false;
  if (!JS_DefineProperty(cx, init, "changed", changedValue, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "deleted", deletedValue, JSPROP_ENUMERATE)) return false;
  return FireRuntimeEvent(s, target, "CookieChangeEvent", "change", Iface::CookieChange,
                          Kind::CookieChange, init);
}

// The Window facade hands the already-cloned message here; delivery is the same
// dispatch path as dispatchEvent on the target's resolved event path.
bool Realm::FireMessage(JS::HandleObject target, JS::HandleValue data, const char* origin,
                        JS::HandleObject sender, JS::HandleObject ports) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject init(cx, JS_NewPlainObject(cx));
  JS::RootedString origin_text(cx, JS_NewStringCopyZ(cx, origin ? origin : ""));
  JS::RootedValue source(cx, JS::ObjectOrNullValue(sender));
  JS::RootedObject prototype(cx);
  JS::RootedValue payload(cx, data);
  JS::RootedValue transferred(cx, JS::ObjectOrNullValue(ports));
  if (!JS_WrapValue(cx, &transferred)) return false;
  if (!JS_WrapValue(cx, &source) || !JS_WrapValue(cx, &payload) ||
      !ReadInstalledEventPrototype(state, Iface::Message, &prototype)) return false;
  if (!init || !origin_text ||
      !JS_DefineProperty(cx, init, "data", payload, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "origin", origin_text, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "source", source, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "ports", transferred, JSPROP_ENUMERATE)) return false;
  return FireRuntimeEvent(state, target, "MessageEvent", "message", Iface::Message,
                          Kind::Message, init, prototype);
}

bool Realm::FireConnect(JS::HandleObject target, JS::HandleObject ports) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject init(cx, JS_NewPlainObject(cx)), prototype(cx);
  JS::RootedString origin_text(cx, JS_NewStringCopyZ(cx, ""));
  JS::RootedString data_text(cx, JS_NewStringCopyZ(cx, ""));
  JS::RootedValue transferred(cx, JS::ObjectOrNullValue(ports));
  JS::RootedValue first_port(cx, JS::NullValue());
  if (!JS_GetElement(cx, ports, 0, &first_port)) return false;
  JS::RootedValue source(cx, first_port);
  if (!init || !origin_text || !data_text || !JS_WrapValue(cx, &transferred) ||
      !JS_WrapValue(cx, &source) ||
      !ReadInstalledEventPrototype(state, Iface::Message, &prototype) ||
      !JS_DefineProperty(cx, init, "data", data_text, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "origin", origin_text, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "source", source, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "ports", transferred, JSPROP_ENUMERATE)) return false;
  return FireRuntimeEvent(state, target, "MessageEvent", "connect", Iface::Message,
                          Kind::Message, init, prototype);
}

bool Realm::FireNamedMessage(JS::HandleObject target, const std::u16string& type, JS::HandleValue data,
                             const char* origin, const std::u16string& lastEventId) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject init(cx, JS_NewPlainObject(cx)), prototype(cx);
  JS::RootedString origin_text(cx, JS_NewStringCopyZ(cx, origin ? origin : ""));
  JS::RootedString id(cx, JS_NewUCStringCopyN(cx, lastEventId.data(), lastEventId.size()));
  JS::RootedValue payload(cx, data);
  if (!init || !origin_text || !id || !JS_WrapValue(cx, &payload) ||
      !ReadInstalledEventPrototype(state, Iface::Message, &prototype) ||
      !JS_DefineProperty(cx, init, "data", payload, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "origin", origin_text, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "lastEventId", id, JSPROP_ENUMERATE)) return false;
  std::string name;
  for (char16_t c : type) if (c > 0 && c < 0x100) name.push_back(char(c));
  if (name.size() != type.size()) return true;
  return FireRuntimeEvent(state, target, "MessageEvent", name.c_str(), Iface::Message,
                          Kind::Message, init, prototype);
}

// History traversal fires one popstate at the window with the restored entry's
// structured-clone state (which may be any value, including null).
bool Realm::FirePopState(JS::HandleObject target, JS::HandleValue state_value) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject init(cx, JS_NewPlainObject(cx));
  if (!init || !JS_DefineProperty(cx, init, "state", state_value, JSPROP_ENUMERATE))
    return false;
  return FireRuntimeEvent(state, target, "PopStateEvent", "popstate", Iface::PopState,
                          Kind::PopState, init);
}

// The OfflineAudioContext render fires one complete carrying the AudioBuffer it
// rendered (T13c); the Web Audio owner hands the buffer over.
bool Realm::FireOfflineAudioComplete(JS::HandleObject target, JS::HandleValue buffer) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject init(cx, JS_NewPlainObject(cx));
  if (!init || !JS_DefineProperty(cx, init, "renderedBuffer", buffer, JSPROP_ENUMERATE))
    return false;
  return FireRuntimeEvent(state, target, "OfflineAudioCompletionEvent", "complete",
                          Iface::OfflineAudioComplete, Kind::OfflineAudioComplete,
                          init);
}

// The Location hash setter fires one hashchange with the old and new absolute
// URLs; the document is not reloaded.
bool Realm::FireHashChange(JS::HandleObject target, const char* oldURL, const char* newURL) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject init(cx, JS_NewPlainObject(cx));
  JS::RootedString old_text(cx, JS_NewStringCopyZ(cx, oldURL ? oldURL : ""));
  JS::RootedString new_text(cx, JS_NewStringCopyZ(cx, newURL ? newURL : ""));
  if (!init || !old_text || !new_text ||
      !JS_DefineProperty(cx, init, "oldURL", old_text, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "newURL", new_text, JSPROP_ENUMERATE)) return false;
  return FireRuntimeEvent(state, target, "HashChangeEvent", "hashchange", Iface::HashChange,
                          Kind::HashChange, init);
}
bool Realm::FireBeforeUnload(JS::HandleObject target) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject init(cx, JS_NewPlainObject(cx));
  if (!init || !JS_DefineProperty(cx, init, "cancelable", JS::TrueHandleValue, JSPROP_ENUMERATE))
    return false;
  return FireRuntimeEvent(state, target, "BeforeUnloadEvent", "beforeunload", Iface::BeforeUnload,
                          Kind::BeforeUnload, init);
}
bool Realm::FirePageTransition(JS::HandleObject target, const char* type, bool persisted) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject init(cx, JS_NewPlainObject(cx));
  JS::RootedValue value(cx, JS::BooleanValue(persisted));
  if (!init || !JS_DefineProperty(cx, init, "persisted", value, JSPROP_ENUMERATE)) return false;
  return FireRuntimeEvent(state, target, "PageTransitionEvent", type, Iface::PageTransition,
                          Kind::PageTransition, init);
}
bool Realm::FirePolicyViolation(JS::HandleObject target, JS::HandleObject init) {
  // The init dictionary is built by the Content Security owner in this realm;
  // the event object, slots and dispatch stay this owner's one runtime path.
  return FireRuntimeEvent(*state_, target, "SecurityPolicyViolationEvent", "securitypolicyviolation",
                          Iface::PolicyViolation, Kind::PolicyViolation, init);
}
// The Navigation owner's trusted NavigateEvent (cancelable; `allowed` reports a
// cancel) and NavigationCurrentEntryChangeEvent, built from init dictionaries
// that owner made in this realm (docs/owners/BROWSER_ENVIRONMENT.md).
bool Realm::FireNavigate(JS::HandleObject target, JS::HandleObject init, bool* allowed) {
  return FireRuntimeEvent(*state_, target, "NavigateEvent", "navigate", Iface::Navigate, Kind::Navigate,
                          init, nullptr, -1, true, allowed);
}
bool Realm::FireIceCandidate(JS::HandleObject target, JS::HandleValue candidate) {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.page);
  JS::RootedObject init(cx, JS_NewPlainObject(cx));
  JS::RootedValue value(cx, candidate);
  if (!init || !JS_WrapValue(cx, &value) || !JS_DefineProperty(cx, init, "candidate", value, JSPROP_ENUMERATE))
    return false;
  return FireRuntimeEvent(s, target, "RTCPeerConnectionIceEvent", "icecandidate", Iface::RtcIce, Kind::RtcIce, init);
}
bool Realm::FireNavigationEntryChange(JS::HandleObject target, JS::HandleObject init) {
  return FireRuntimeEvent(*state_, target, "NavigationCurrentEntryChangeEvent", "currententrychange",
                          Iface::NavigationEntryChange, Kind::NavigationEntryChange, init);
}
bool Realm::FireSpeech(JS::HandleObject target, const char* type, JS::HandleObject init,
                       bool error) {
  return FireRuntimeEvent(*state_, target, error ? "SpeechSynthesisErrorEvent" : "SpeechSynthesisEvent",
                          type, error ? Iface::SpeechError : Iface::Speech,
                          error ? Kind::SpeechError : Kind::SpeechTrusted, init);
}
}  // namespace ruyijs::interaction
