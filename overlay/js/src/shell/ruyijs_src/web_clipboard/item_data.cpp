// ClipboardItem data: each entry waits on the promise its constructor value was
// converted to, then settles every pending getType() the way the donor's
// ItemEntry does (dom/events/ClipboardItem.cpp): a Blob is returned as is, a
// DOMString becomes a new Blob of the entry's type on every call.
#include "web_clipboard/internal.h"
#include "domain/clipboard/clipboard_type.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/ValueArray.h"

namespace ruyijs::web_clipboard {
namespace {
// NS_ERROR_DOM_DATA_ERR's message, used for a promise that was still waiting.
constexpr const char16_t* kDataError = u"Data provided to an operation does not meet requirements";

State& OwnerOf(JSObject* item) {
  return *static_cast<State*>(JS::GetReservedSlot(item, ItemOwner).toPrivate());
}
// The value a Loaded entry resolves getType() with.
bool Settled(State& state, JS::HandleObject entry, JS::MutableHandleValue out) {
  JSContext* cx = state.cx;
  JS::RootedValue data(cx, JS::GetReservedSlot(entry, EntryData));
  if (data.isObject()) { out.set(data); return true; }
  JS::RootedObject parts(cx, JS::NewArrayObject(cx, 1)), bag(cx, JS_NewPlainObject(cx));
  JS::RootedValue type(cx, JS::GetReservedSlot(entry, EntryType));
  if (!parts || !bag || !JS_SetElement(cx, parts, 0, data) ||
      !JS_SetProperty(cx, bag, "type", type)) return false;
  JS::RootedValueArray<2> values(cx);
  values[0].setObject(*parts);
  values[1].setObject(*bag);
  JS::RootedValue constructor(cx, JS::ObjectValue(*state.blob));
  JS::RootedObject blob(cx);
  if (!JS::Construct(cx, constructor, values, &blob)) return false;
  out.setObject(*blob);
  return true;
}
bool Settle(State& state, JS::HandleObject entry, JS::HandleObject promise) {
  JSContext* cx = state.cx;
  JS::RootedValue value(cx);
  if (JS::GetReservedSlot(entry, EntryStatus).toInt32() == Loaded) {
    return Settled(state, entry, &value) && JS::ResolvePromise(cx, promise, value);
  }
  return NewDomException(state, kDataError, "DataError", &value) &&
         JS::RejectPromise(cx, promise, value);
}
bool Finish(JSContext* cx, const JS::CallArgs& args, EntryState status, JS::HandleValue data) {
  State& state = *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  JS::RootedObject entry(cx, &js::GetFunctionNativeReserved(&args.callee(), 1).toObject());
  JS::SetReservedSlot(entry, EntryStatus, JS::Int32Value(status));
  JS::SetReservedSlot(entry, EntryData, data);
  JS::RootedObject pending(cx, &JS::GetReservedSlot(entry, EntryPending).toObject());
  JS::SetReservedSlot(entry, EntryPending, JS::UndefinedValue());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, pending, &length)) return false;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue promise(cx);
    if (!JS_GetElement(cx, pending, i, &promise)) return false;
    JS::RootedObject waiting(cx, &promise.toObject());
    if (!Settle(state, entry, waiting)) return false;
  }
  args.rval().setUndefined();
  return true;
}
// The resolved value becomes (DOMString or Blob); a value that cannot be
// converted fails the entry like a rejection.
bool Fulfilled(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  JS::RootedValue data(cx, args.get(0));
  if (!(state.blobBrand && state.blobBrand(data))) {
    JS::RootedString text(cx, data.isSymbol() ? nullptr : JS::ToString(cx, data));
    if (!text) {
      JS_ClearPendingException(cx);
      return Finish(cx, args, Failed, JS::UndefinedHandleValue);
    }
    data.setString(text);
  }
  return Finish(cx, args, Loaded, data);
}
bool Rejected(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return Finish(cx, args, Failed, JS::UndefinedHandleValue);
}
JSObject* Reaction(State& state, JSNative native, JS::HandleObject entry) {
  JSFunction* function = js::NewFunctionWithReserved(state.cx, native, 1, 0, nullptr);
  if (!function) return nullptr;
  JSObject* object = JS_GetFunctionObject(function);
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(&state));
  js::SetFunctionNativeReserved(object, 1, JS::ObjectValue(*entry));
  return object;
}
}  // namespace

bool LoadEntry(State& state, JS::HandleObject entry, JS::HandleObject promise) {
  JSContext* cx = state.cx;
  JS::RootedObject fulfilled(cx, Reaction(state, Fulfilled, entry));
  JS::RootedObject rejected(cx, fulfilled ? Reaction(state, Rejected, entry) : nullptr);
  return rejected &&
         JS::AddPromiseReactionsIgnoringUnhandledRejection(cx, promise, fulfilled, rejected);
}

bool NewDomException(State& state, const std::u16string& message, const char* name,
                     JS::MutableHandleValue error) {
  JSContext* cx = state.cx;
  JS::RootedValueArray<2> values(cx);
  JS::RootedString text(cx, NewString(cx, message)), label(cx, JS_NewStringCopyZ(cx, name));
  if (!text || !label) return false;
  values[0].setString(text);
  values[1].setString(label);
  JS::RootedValue constructor(cx, JS::ObjectValue(*state.exception));
  JS::RootedObject object(cx);
  if (!JS::Construct(cx, constructor, values, &object)) return false;
  error.setObject(*object);
  return true;
}

bool RejectPending(State& state, JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  JS::RootedValue error(cx);
  if (!JS_GetPendingException(cx, &error)) return false;
  JS_ClearPendingException(cx);
  return state.kit.RejectWith(error, result);
}

// [NewObject] Promise<Blob> getType(DOMString type): every failure, including
// an incompatible receiver, settles the returned promise instead of throwing.
bool GetType(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& caller = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx, ItemReceiver(cx, args.thisv(), "getType"));
  if (!self) return RejectPending(caller, args.rval());
  State& state = OwnerOf(self);
  std::u16string input;
  if (!argc) {
    WebKit::Fail(cx, "ClipboardItem.getType: At least 1 argument required, but only 0 passed");
    return RejectPending(caller, args.rval());
  }
  if (!ToUtf16(cx, args[0], &input)) return RejectPending(caller, args.rval());
  auto parsed = domain::clipboard::ParseClipboardType(input);
  if (!parsed.valid) {
    ThrowType(cx, u"ClipboardItem.getType: Type '" + input + u"' is not a valid type");
    return RejectPending(caller, args.rval());
  }
  {
    JSAutoRealm realm(cx, state.kit.page);
    JS::RootedObject entries(cx, &JS::GetReservedSlot(self, ItemEntries).toObject());
    JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
    uint32_t length = 0;
    if (!promise || !JS::GetArrayLength(cx, entries, &length)) return false;
    JS::RootedValue value(cx);
    bool found = false;
    for (uint32_t i = 0; i < length && !found; ++i) {
      if (!JS_GetElement(cx, entries, i, &value)) return false;
      JS::RootedObject entry(cx, &value.toObject());
      std::u16string type;
      JS::RootedValue typeValue(cx, JS::GetReservedSlot(entry, EntryType));
      if (!ToUtf16(cx, typeValue, &type)) return false;
      if (type != parsed.type) continue;
      found = true;
      const int32_t status = JS::GetReservedSlot(entry, EntryStatus).toInt32();
      if (status == Loading) {
        JS::RootedObject pending(cx, &JS::GetReservedSlot(entry, EntryPending).toObject());
        JS::RootedValue held(cx, JS::ObjectValue(*promise));
        uint32_t count = 0;
        if (!JS::GetArrayLength(cx, pending, &count) || !JS_SetElement(cx, pending, count, held))
          return false;
      } else if (status == Loaded) {
        if (!Settle(state, entry, promise)) return false;
      } else {
        if (!NewDomException(state, u"The data for type '" + type + u"' was not found",
                             "DataError", &value) ||
            !JS::RejectPromise(cx, promise, value)) return false;
      }
    }
    if (!found && (!NewDomException(state, u"The type '" + input + u"' was not found",
                                    "NotFoundError", &value) ||
                   !JS::RejectPromise(cx, promise, value))) return false;
    args.rval().setObject(*promise);
  }
  return JS_WrapValue(cx, args.rval());
}
}  // namespace ruyijs::web_clipboard
