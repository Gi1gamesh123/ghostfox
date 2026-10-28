#include "web_messaging/state.h"
#include "interaction/realm.h"
#include "document_model/exception.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_messaging {
thread_local State* activeState = nullptr;
const JSClass kPortClass = {"MessagePort", JSCLASS_HAS_RESERVED_SLOTS(PortSlots)};
const JSClass kChannelClass = {"MessageChannel", JSCLASS_HAS_RESERVED_SLOTS(2)};
Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events)
    : state_(std::make_unique<State>(cx, page, helpers, events)) {
  state_->previous = activeState; activeState = state_.get();
}
Realm::~Realm() {
  Retire();
  State** link = &activeState;
  while (*link && *link != state_.get()) link = &(*link)->previous;
  if (*link) *link = state_->previous;
}
void Realm::SetTaskPort(TaskPort port, void* context) {
  state_->task = port; state_->taskContext = context;
}
void Realm::Retire() {
  state_->active = false;
  for (auto& record : state_->records) if (record->endpoint) {
    Disconnect(*record->endpoint);
    record->endpoint->receiver = nullptr;
    record->endpoint->pending.clear();
  }
  state_->task = nullptr; state_->taskContext = nullptr;
  state_->relay = nullptr; state_->relayContext = nullptr;
  state_->remoteClose = nullptr; state_->remoteCloseContext = nullptr;
}
bool CloneError(State& state) {
  return document_model::ThrowException(state.cx, state.exception,
      "DataCloneError", "The object could not be cloned");
}
bool Realm::Install() {
  State& s = *state_; JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedObject target(cx, s.events.prototype());
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, s.kit.page, "DOMException", &value) || !value.isObject()) return false;
  JS::RootedObject exception(cx, &value.toObject());
  if (!JS_GetProperty(cx, exception, "prototype", &value) || !value.isObject()) return false;
  s.exception = &value.toObject();
  if (!JS_GetProperty(cx, s.kit.page, "Event", &value) || !value.isObject()) return false;
  JS::RootedObject event(cx, &value.toObject());
  if (!JS_GetProperty(cx, event, "prototype", &value) || !value.isObject()) return false;
  event = &value.toObject();
  if (!JS_GetProperty(cx, event, "preventDefault", &s.prevent)) return false;
  if (!JS_GetProperty(cx, target, "addEventListener", &s.add) ||
      !JS_GetProperty(cx, target, "removeEventListener", &s.remove) ||
      !s.kit.Define("MessageChannel", &kChannelClass, nullptr, Construct, &s.channel) ||
      !s.kit.Accessor(s.channel, "MessageChannel", "port1", GetPort, nullptr, 0) ||
      !s.kit.Accessor(s.channel, "MessageChannel", "port2", GetPort, nullptr, 1) ||
      !s.kit.Define("MessagePort", &kPortClass, target, WebKit::Reject, &s.port) ||
      !s.kit.Method(s.port, "MessagePort", "postMessage", Post, 1, 0) ||
      !s.kit.Method(s.port, "MessagePort", "start", Start, 0, 0) ||
      !s.kit.Method(s.port, "MessagePort", "close", Close, 0, 0) ||
      !s.kit.Accessor(s.port, "MessagePort", "onmessage", GetHandler, SetHandler, 0) ||
      !s.kit.Accessor(s.port, "MessagePort", "onmessageerror", GetHandler, SetHandler, 1)) return false;
  for (WebFace* face : {&s.channel, &s.port}) {
    JS::RootedValue constructor(cx, JS::ObjectValue(*face->constructor));
    if (!JS_DeleteProperty(cx, face->prototype, "constructor") ||
        !JS_DefineProperty(cx, face->prototype, "constructor", constructor, 0)) return false;
  }
  return true;
}
Record* Unwrap(JSObject* object) {
  object = js::CheckedUnwrapStatic(object);
  if (!object || JS::GetClass(object) != &kPortClass) return nullptr;
  auto value = JS::GetReservedSlot(object, PortRecord);
  return value.isUndefined() ? nullptr : static_cast<Record*>(value.toPrivate());
}
Record* Receiver(JSContext* cx, JS::HandleValue value) {
  Record* record = value.isObject() ? Unwrap(&value.toObject()) : nullptr;
  if (!record) WebKit::Fail(cx, "Object does not implement interface MessagePort");
  return record;
}
JSObject* NewPort(State& s, std::shared_ptr<Endpoint> endpoint) {
  JS::RootedObject port(s.cx, s.kit.New(&kPortClass, s.port.prototype));
  if (!port || !s.events.Attach(port)) return nullptr;
  auto record = std::make_unique<Record>(s.cx, s, port, std::move(endpoint));
  record->endpoint->receiver = record.get();
  for (unsigned i = OnMessage; i < PortSlots; ++i) JS::SetReservedSlot(port, i, JS::NullValue());
  JS::SetReservedSlot(port, PortRecord, JS::PrivateValue(record.get()));
  s.records.push_back(std::move(record));
  return port;
}
bool Construct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject prototype(cx);
  if (!s.kit.ConstructProto(args, s.channel.prototype, &prototype)) return false;
  JS::RootedObject channel(cx, s.kit.New(&kChannelClass, prototype));
  auto first = std::make_shared<Endpoint>(), second = std::make_shared<Endpoint>();
  first->peer = second; second->peer = first;
  JS::RootedObject p1(cx, NewPort(s, first)), p2(cx, NewPort(s, second));
  if (!channel || !p1 || !p2) return false;
  JS::SetReservedSlot(channel, 0, JS::ObjectValue(*p1));
  JS::SetReservedSlot(channel, 1, JS::ObjectValue(*p2));
  args.rval().setObject(*channel); return true;
}
bool GetPort(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx, args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!self || JS::GetClass(self) != &kChannelClass)
    return WebKit::Fail(cx, "Object does not implement interface MessageChannel");
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return JS_WrapValue(cx, args.rval());
}
}
