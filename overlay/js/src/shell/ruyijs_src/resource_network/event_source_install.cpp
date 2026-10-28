// The EventSource interface (dom/webidl/EventSource.webidl): constructor, url,
// withCredentials, readyState, close() and the three event handler attributes.
#include "resource_network/event_source.h"
#include "resource_network/request.h"
#include "runtime/native_constructor_trace.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/ValueArray.h"

namespace ruyijs::resource_network {
const JSClass kEventSourceClass = {"EventSource", JSCLASS_HAS_RESERVED_SLOTS(ESlots)};
namespace {
constexpr const char* kHandlers[] = {"open", "message", "error"};
constexpr const char* kFields[] = {"url", "withCredentials", "readyState"};
State& Owner(const JS::CallArgs& args) { return *static_cast<State*>(WebKit::Owner(args)); }
bool Receiver(JSContext* cx, const JS::CallArgs& args, const std::string& member, JS::MutableHandleObject self) {
  return WebKit::Check(cx, args.thisv(), &kEventSourceClass, self,
      ("'" + member + "' called on an object that does not implement interface EventSource.").c_str());
}
bool SyntaxError(JSContext* cx, State& state, const std::string& message) {
  JS::RootedValue constructor(cx), error(cx);
  JS::RootedObject created(cx);
  if (!JS_GetProperty(cx, state.page, "DOMException", &constructor) || !constructor.isObject())
    return WebKit::Fail(cx, message.c_str());
  JS::RootedValueArray<2> values(cx);
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(message.data(), message.size())));
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, "SyntaxError"));
  if (!text || !name) return false;
  values[0].setString(text);
  values[1].setString(name);
  if (!JS::Construct(cx, constructor, values, &created)) return false;
  error.setObject(*created);
  JS_SetPendingException(cx, error);
  return false;
}
bool Construct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  State& state = Owner(args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "EventSource constructor: 'new' is required");
  if (!argc) return WebKit::Fail(cx, "EventSource constructor: At least 1 argument required, but only 0 passed");
  std::string input, url;
  if (!Convert(cx, args[0], &input)) return false;
  bool credentials = false;
  if (args.get(1).isObject()) {
    JS::RootedObject init(cx, &args[1].toObject());
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, init, "withCredentials", &value)) return false;
    credentials = JS::ToBoolean(value);
  } else if (!args.get(1).isNullOrUndefined()) {
    return WebKit::Fail(cx, "EventSource constructor: Value can't be converted to a dictionary.");
  }
  if (!RequestURL(state, input, &url)) {
    JS_ClearPendingException(cx);
    return SyntaxError(cx, state, "An invalid or illegal string was specified");
  }
  JS::RootedObject prototype(cx);
  if (!state.kit.ConstructProto(args, state.eventSource.prototype, &prototype)) return false;
  JS::RootedObject self(cx, state.kit.New(&kEventSourceClass, prototype));
  JS::RootedValue href(cx), empty(cx, JS_GetEmptyStringValue(cx));
  if (!self || !Value(cx, url, &href) || !state.events || !state.events->Attach(self)) return false;
  JS::SetReservedSlot(self, EUrl, href);
  JS::SetReservedSlot(self, ECredentials, JS::BooleanValue(credentials));
  JS::SetReservedSlot(self, EState, JS::Int32Value(0));
  for (uint32_t slot : {EOnOpen, EOnMessage, EOnError}) JS::SetReservedSlot(self, slot, JS::NullValue());
  JS::SetReservedSlot(self, ELastId, empty);
  JS::SetReservedSlot(self, ERetry, JS::NumberValue(5000));  // Firefox's default reconnection time
  JS::SetReservedSlot(self, EGeneration, JS::NumberValue(0));
  JS::SetReservedSlot(self, EOwner, JS::PrivateValue(&state));
  state.Observe("EventSource", "constructor", int(ApiAccess::Construct));
  if (!EventSourceConnect(state, self)) return false;
  args.rval().setObject(*self);
  return trace.Finish(true);
}
bool Field(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, std::string("get ") + kFields[slot], &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, uint32_t(slot)));
  return true;
}
bool Close(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, "close", &self)) return false;
  JS::SetReservedSlot(self, EState, JS::Int32Value(2));
  JS::SetReservedSlot(self, EGeneration, JS::NumberValue(JS::GetReservedSlot(self, EGeneration).toNumber() + 1));
  args.rval().setUndefined();
  return true;
}
// on<type>: one listener added and removed through the instance's EventTarget.
bool Handler(JSContext* cx, const JS::CallArgs& args, bool setter) {
  const int32_t index = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, std::string(setter ? "set on" : "get on") + kHandlers[index], &self)) return false;
  const uint32_t slot = EOnOpen + uint32_t(index);
  if (!setter) { args.rval().set(JS::GetReservedSlot(self, slot)); return JS_WrapValue(cx, args.rval()); }
  JS::RootedValue previous(cx, JS::GetReservedSlot(self, slot)), desired(cx, args.get(0)), function(cx), ignored(cx);
  if (!desired.isObject() || !JS::IsCallable(&desired.toObject())) desired.setNull();
  JS::RootedValueArray<2> parameters(cx);
  JS::RootedString type(cx, JS_NewStringCopyZ(cx, kHandlers[index]));
  if (!type) return false;
  parameters[0].setString(type);
  JS::RootedValue receiver(cx, JS::ObjectValue(*self));
  for (int operation = 0; operation < 2; ++operation) {
    parameters[1].set(operation ? desired : previous);
    if (!parameters[1].isObject()) continue;
    if (!JS_GetProperty(cx, self, operation ? "addEventListener" : "removeEventListener", &function) ||
        !JS::Call(cx, receiver, function, parameters, &ignored)) return false;
  }
  JS::SetReservedSlot(self, slot, desired);
  args.rval().setUndefined();
  return true;
}
bool HandlerGet(JSContext* cx, unsigned argc, JS::Value* vp) { return Handler(cx, JS::CallArgsFromVp(argc, vp), false); }
bool HandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) { return Handler(cx, JS::CallArgsFromVp(argc, vp), true); }
}  // namespace

bool InstallEventSource(State& state) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject eventTarget(cx, state.events ? state.events->prototype() : nullptr);
  if (!eventTarget || !state.kit.Define("EventSource", &kEventSourceClass, eventTarget, Construct, &state.eventSource, 1) ||
      !state.kit.Method(state.eventSource, "EventSource", "close", Close, 0, 0)) return false;
  for (int32_t i = 0; i < 3; ++i)
    if (!state.kit.Accessor(state.eventSource, "EventSource", kFields[i], Field, nullptr, i)) return false;
  for (int32_t i = 0; i < 3; ++i) {
    const std::string name = std::string("on") + kHandlers[i];
    if (!state.kit.Accessor(state.eventSource, "EventSource", name.c_str(), HandlerGet, HandlerSet, i)) return false;
  }
  const char* names[] = {"CONNECTING", "OPEN", "CLOSED"};
  for (int i = 0; i < 3; ++i) {
    const unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
    // The prototype copy is sealed by the binding order pass, after it is placed last.
    if (!JS_DefineProperty(cx, state.eventSource.constructor, names[i], i, flags) ||
        !JS_DefineProperty(cx, state.eventSource.prototype, names[i], i, flags & ~JSPROP_PERMANENT)) return false;
  }
  return true;
}
}  // namespace ruyijs::resource_network
