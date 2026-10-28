// BroadcastChannel members. postMessage clones synchronously in the sender
// realm, reads one copy into each same-origin, same-name receiver realm and
// queues one delivery task per receiver; a receiver closed meanwhile gets none.
#include "web_broadcast/internal.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/StructuredClone.h"
#include "js/ValueArray.h"
#include "js/Wrapper.h"
#include "mozilla/Range.h"

namespace ruyijs::web_broadcast {
const JSClass kChannelClass = {"BroadcastChannel", JSCLASS_HAS_RESERVED_SLOTS(ChannelSlotCount)};
namespace {
State& Self(const JS::CallArgs& args) { return *static_cast<State*>(WebKit::Owner(args)); }
bool Receiver(JSContext* cx, JS::HandleValue self, const char* member, JS::MutableHandleObject out) {
  JSObject* object = self.isObject() ? js::CheckedUnwrapStatic(&self.toObject()) : nullptr;
  if (!object || JS::GetClass(object) != &kChannelClass)
    return WebKit::Fail(cx, (std::string("'") + member +
        "' called on an object that does not implement interface BroadcastChannel.").c_str());
  out.set(object);
  return true;
}
bool ThrowDOM(State& state, const char* name, const char* message) {
  JSContext* cx = state.cx;
  JS::RootedValue constructor(cx);
  if (!JS_GetProperty(cx, state.kit.page, "DOMException", &constructor) || !constructor.isObject())
    return WebKit::Fail(cx, message);
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message)), label(cx, JS_NewStringCopyZ(cx, name));
  if (!text || !label) return false;
  JS::RootedValueArray<2> values(cx);
  values[0].setString(text);
  values[1].setString(label);
  JS::RootedObject error(cx);
  if (!JS::Construct(cx, constructor, values, &error)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*error));
  JS_SetPendingException(cx, value);
  return false;
}
void CloneError(JSContext*, uint32_t, void* closure, const char*) { *static_cast<bool*>(closure) = true; }
const JSStructuredCloneCallbacks kCallbacks = {nullptr, nullptr, CloneError};
uint64_t Id(JSObject* channel) { return uint64_t(JS::GetReservedSlot(channel, ChannelId).toNumber()); }
// Delivery task: reserved slot 0 is the receiver realm's State, slot 1 the
// [channel, data, origin] record read into that realm.
bool Deliver(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  JS::RootedObject record(cx, &js::GetFunctionNativeReserved(&args.callee(), 1).toObject());
  JS::RootedValue channel(cx), data(cx), origin(cx);
  args.rval().setUndefined();
  if (!JS_GetElement(cx, record, 0, &channel) || !JS_GetElement(cx, record, 1, &data) ||
      !JS_GetElement(cx, record, 2, &origin)) return false;
  JS::RootedObject target(cx, &channel.toObject());
  if (!state.registry->Find(Id(target))) return true;
  JS::RootedString text(cx, origin.toString());
  JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, text);
  JS::RootedObject source(cx);
  return utf8 && state.events.FireMessage(target, data, utf8.get(), source);
}
}  // namespace
bool QueueDelivery(Entry& entry, JSAutoStructuredCloneBuffer& buffer) {
  State& target = *entry.owner;
  JSContext* cx = target.cx;
  if (!target.taskPort) return true;
  JSAutoRealm realm(cx, target.kit.page);
  JS::CloneDataPolicy policy;
  bool cloneError = false;
  JS::RootedValue data(cx);
  if (!buffer.read(cx, &data, policy, &kCallbacks, &cloneError)) return false;
  JS::RootedString origin(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(entry.origin.data(),
                                                                         entry.origin.size())));
  JS::RootedObject record(cx, JS::NewArrayObject(cx, 3));
  JS::RootedValue channel(cx, JS::ObjectValue(*entry.channel));
  if (!origin || !record || !JS_SetElement(cx, record, 0, channel) ||
      !JS_SetElement(cx, record, 1, data) || !JS_SetElement(cx, record, 2, origin)) return false;
  JSFunction* function = js::NewFunctionWithReserved(cx, Deliver, 0, 0, "BroadcastChannel delivery");
  if (!function) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(&target));
  js::SetFunctionNativeReserved(task, 1, JS::ObjectValue(*record));
  return target.taskPort(target.taskContext, task);
}
bool Construct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "BroadcastChannel constructor: 'new' is required");
  if (!args.length())
    return WebKit::Fail(cx, "BroadcastChannel constructor: At least 1 argument required, but only 0 passed");
  JS::RootedString name(cx, JS::ToString(cx, args[0]));
  JS::RootedObject prototype(cx);
  if (!name || !state.kit.ConstructProto(args, state.face.prototype, &prototype)) return false;
  std::u16string key(JS_GetStringLength(name), u'\0');
  if (!JS_CopyStringChars(cx, mozilla::Range<char16_t>(key.data(), key.size()), name)) return false;
  JS::RootedValue originValue(cx);
  std::string origin;
  if (!state.environment.Origin(&originValue)) return false;
  if (originValue.isString()) {
    JS::RootedString text(cx, originValue.toString());
    JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, text);
    if (!utf8) return false;
    origin = utf8.get();
  }
  JS::RootedObject channel(cx, state.kit.New(&kChannelClass, prototype));
  if (!channel || !state.events.Attach(channel)) return false;
  const uint64_t id = state.registry->next++;
  JS::SetReservedSlot(channel, ChannelName, JS::StringValue(name));
  JS::SetReservedSlot(channel, ChannelId, JS::NumberValue(double(id)));
  JS::SetReservedSlot(channel, ChannelMessage, JS::NullValue());
  JS::SetReservedSlot(channel, ChannelMessageError, JS::NullValue());
  state.registry->entries.push_back(std::make_unique<Entry>(cx, id, &state, std::move(origin),
                                                            std::move(key), channel));
  args.rval().setObject(*channel);
  return true;
}
bool PostMessage(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), "postMessage", &self)) return false;
  if (!args.length())
    return WebKit::Fail(cx, "BroadcastChannel.postMessage: At least 1 argument required, but only 0 passed");
  Entry* sender = state.registry->Find(Id(self));
  if (!sender) return ThrowDOM(state, "InvalidStateError",
      "An attempt was made to use an object that is not, or is no longer, usable");
  bool cloneError = false;
  JSAutoStructuredCloneBuffer buffer(JS::StructuredCloneScope::SameProcess, &kCallbacks, &cloneError);
  if (!buffer.write(cx, args[0], &kCallbacks, &cloneError)) {
    if (!cloneError && JS_IsExceptionPending(cx)) return false;
    JS_ClearPendingException(cx);
    return ThrowDOM(state, "DataCloneError", "The object could not be cloned");
  }
  const uint64_t from = sender->id;
  const std::string origin = sender->origin;
  const std::u16string name = sender->name;
  std::vector<uint64_t> targets;
  for (auto& entry : state.registry->entries)
    if (entry->id != from && entry->origin == origin && entry->name == name) targets.push_back(entry->id);
  for (uint64_t id : targets)
    if (Entry* entry = state.registry->Find(id); entry && !QueueDelivery(*entry, buffer)) return false;
  // Other processes of the Context (Workers) receive it through the relay port.
  if (state.registry->relay && !state.registry->relay(state.registry->relayContext, origin, name, args[0]))
    return false;
  args.rval().setUndefined();
  return true;
}
bool Close(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), "close", &self)) return false;
  state.registry->Remove(Id(self));
  args.rval().setUndefined();
  return true;
}
bool NameGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), "get name", &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, ChannelName));
  return JS_WrapValue(cx, args.rval());
}
}
