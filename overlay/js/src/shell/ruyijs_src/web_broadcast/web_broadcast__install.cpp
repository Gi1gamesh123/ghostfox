// The one production installer of BroadcastChannel and its event handler
// attributes, which register on the Interaction owner's one listener tree.
#include "web_broadcast/internal.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/ValueArray.h"
#include "js/Wrapper.h"

namespace ruyijs::web_broadcast {
namespace {
const char* const kTypes[2] = {"message", "messageerror"};
JSObject* Channel(JSContext* cx, JS::HandleValue self, const char* member) {
  JSObject* object = self.isObject() ? js::CheckedUnwrapStatic(&self.toObject()) : nullptr;
  if (object && JS::GetClass(object) == &kChannelClass) return object;
  WebKit::Fail(cx, (std::string("'") + member +
      "' called on an object that does not implement interface BroadcastChannel.").c_str());
  return nullptr;
}
}
bool HandlerGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t which = WebKit::Slot(args);
  JS::RootedObject self(cx, Channel(cx, args.thisv(), which ? "get onmessageerror" : "get onmessage"));
  if (!self) return false;
  args.rval().set(JS::GetReservedSlot(self, ChannelMessage + which));
  return JS_WrapValue(cx, args.rval());
}
bool HandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  const int32_t which = WebKit::Slot(args);
  JS::RootedObject self(cx, Channel(cx, args.thisv(), which ? "set onmessageerror" : "set onmessage"));
  if (!self) return false;
  JS::RootedValue previous(cx, JS::GetReservedSlot(self, ChannelMessage + which));
  JS::RootedValue desired(cx, args.get(0));
  if (!desired.isObject() || !JS::IsCallable(&desired.toObject())) desired.setNull();
  JSAutoRealm realm(cx, state.kit.page);
  if (!JS_WrapValue(cx, &desired) || !JS_WrapValue(cx, &previous)) return false;
  JS::RootedObject prototype(cx, state.events.prototype());
  JS::RootedValue function(cx), receiver(cx, JS::ObjectValue(*self)), ignored(cx);
  JS::RootedString type(cx, JS_NewStringCopyZ(cx, kTypes[which]));
  if (!type) return false;
  JS::RootedValueArray<2> parameters(cx);
  parameters[0].setString(type);
  for (int operation = 0; operation < 2; ++operation) {
    parameters[1].set(operation ? desired : previous);
    if (!parameters[1].isObject()) continue;
    if (!JS_GetProperty(cx, prototype, operation ? "addEventListener" : "removeEventListener",
                        &function) ||
        !JS::Call(cx, receiver, function, parameters, &ignored)) return false;
  }
  JS::SetReservedSlot(self, ChannelMessage + which, desired);
  args.rval().setUndefined();
  return true;
}
Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events, browser_environment::Environment& environment,
             Realm* parent)
    : state_(std::make_unique<State>(cx, page, helpers, events, environment)) {
  state_->registry = parent ? parent->state_->registry : std::make_shared<Registry>();
}
// A realm's open channels leave the shared registry with it.
Realm::~Realm() {
  Registry& registry = *state_->registry;
  for (size_t i = registry.entries.size(); i-- > 0;)
    if (registry.entries[i]->owner == state_.get()) registry.entries.erase(registry.entries.begin() + i);
}
void Realm::SetTaskPort(TaskPort port, void* context) {
  state_->taskPort = port;
  state_->taskContext = context;
}
bool Realm::Install() {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedObject eventTarget(cx, s.events.prototype());
  WebFace& face = s.face;
  if (!eventTarget ||
      !s.kit.Define("BroadcastChannel", &kChannelClass, eventTarget, Construct, &face) ||
      !JS_DefineProperty(cx, face.constructor, "length", 1, JSPROP_READONLY) ||
      !s.kit.Method(face, "BroadcastChannel", "postMessage", PostMessage, 1, 0) ||
      !s.kit.Method(face, "BroadcastChannel", "close", Close, 0, 0) ||
      !s.kit.Accessor(face, "BroadcastChannel", "name", NameGet, nullptr, 0) ||
      !s.kit.Accessor(face, "BroadcastChannel", "onmessage", HandlerGet, HandlerSet, 0) ||
      !s.kit.Accessor(face, "BroadcastChannel", "onmessageerror", HandlerGet, HandlerSet, 1))
    return false;
  JS::RootedValue constructor(cx, JS::ObjectValue(*face.constructor));
  return JS_DeleteProperty(cx, face.prototype, "constructor") &&
         JS_DefineProperty(cx, face.prototype, "constructor", constructor, 0);
}
}
