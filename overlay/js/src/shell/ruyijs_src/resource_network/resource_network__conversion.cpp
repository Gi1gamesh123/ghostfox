#include "resource_network/state.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/Array.h"
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/Id.h"
#include "js/PropertyAndElement.h"
#include "js/Promise.h"
#include "js/PropertyDescriptor.h"
#include "js/StableStringChars.h"
#include "js/String.h"

namespace ruyijs::resource_network {
bool Convert(JSContext* cx, JS::HandleValue input, std::string* result) {
  JS::RootedString text(cx, JS::ToString(cx, input));
  if (!text) return false;
  JS::AutoStableStringChars characters(cx);
  if (!characters.initTwoByte(cx, text)) return false;
  return Report(cx, ByteString(characters.twoByteChars(), characters.length(), result));
}
bool Value(JSContext* cx, std::string_view source, JS::MutableHandleValue result) {
  JS::RootedString text(cx, JS_NewStringCopyN(cx, source.data(), source.size()));
  if (!text) return false;
  result.setString(text); return true;
}
static bool Sequence(JSContext* cx, JS::ForOfIterator& outer, HeaderDraft* draft) {
  JS::RootedValue row(cx), value(cx);
  while (true) {
    bool done;
    if (!outer.next(&row, &done)) return false;
    if (done) return true;
    if (!row.isObject()) return Error(cx, "Headers entry must be an object sequence");
    JS::ForOfIterator inner(cx);
    if (!inner.init(row) || !Report(cx, DraftTuple(draft))) return false;
    while (true) {
      if (!inner.next(&value, &done)) return false;
      if (done) break;
      std::string converted;
      if (!Convert(cx, value, &converted) || !Report(cx, DraftValue(draft, converted))) return false;
    }
  }
}
static bool Record(JSContext* cx, JS::HandleObject object, HeaderDraft* draft) {
  JS::RootedIdVector keys(cx);
  if (!js::GetPropertyKeys(cx, object, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &keys)) return false;
  for (size_t index = 0; index < keys.length(); ++index) {
    JS::RootedId key(cx, keys[index]);
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> descriptor(cx);
    if (!JS_GetOwnPropertyDescriptorById(cx, object, key, &descriptor)) return false;
    if (descriptor.isNothing() || !descriptor->enumerable()) continue;
    JS::RootedValue name(cx), value(cx);
    std::string converted;
    if (!JS_IdToValue(cx, key, &name) || !Convert(cx, name, &converted) ||
        !Report(cx, DraftTuple(draft)) || !Report(cx, DraftValue(draft, converted)) ||
        !JS_GetPropertyById(cx, object, key, &value) || !Convert(cx, value, &converted) ||
        !Report(cx, DraftValue(draft, converted))) return false;
  }
  return true;
}
State* FunctionOwner(const JS::CallArgs& args) {
  return static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
}
static JSObject* Reserved(State& s, JSNative callback, const char* name, unsigned length,
                          unsigned flags, int32_t slot) {
  JS::RootedFunction function(s.cx, js::NewFunctionWithReserved(s.cx, callback, length, flags, name));
  if (!function) return nullptr;
  JS::RootedObject object(s.cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(&s));
  js::SetFunctionNativeReserved(object, 1, JS::Int32Value(slot));
  return object;
}
JSObject* NativeFunction(State& s, JSNative callback, const char* name, unsigned length,
                         unsigned flags) {
  return Reserved(s, callback, name, length, flags, 0);
}
JSObject* NativeSlot(State& s, JSNative callback, const char* name, unsigned length,
                     int32_t slot) {
  return Reserved(s, callback, name, length, 0, slot);
}
bool Settled(JSContext* cx, JS::HandleValue value, JS::MutableHandleValue result) {
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !JS::ResolvePromise(cx, promise, value)) return false;
  result.setObject(*promise);
  return true;
}
bool Rejected(JSContext* cx, JS::HandleValue error, JS::MutableHandleValue result) {
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !JS::RejectPromise(cx, promise, error)) return false;
  result.setObject(*promise);
  return true;
}
// A TypeError is built by reporting one and taking the pending exception, the
// only public way to obtain the engine's own error object with its name set.
bool RejectWithTypeError(JSContext* cx, JS::HandleObject promise, const char* message) {
  Error(cx, message);
  JS::RootedValue error(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx, &error)) return false;
  JS_ClearPendingException(cx);
  return JS::RejectPromise(cx, promise, error);
}
bool TypeErrorPromise(JSContext* cx, const char* message, JS::MutableHandleValue result) {
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !RejectWithTypeError(cx, promise, message)) return false;
  result.setObject(*promise);
  return true;
}
bool HeaderPairsArgument(JSContext* cx, JS::HandleValue value, HeaderPairs* headers) {
  if (value.isUndefined()) return true;
  if (!value.isObject()) return Error(cx, "headers must be an object or pair list");
  JS::RootedObject object(cx, &value.toObject());
  if (JS::GetClass(object) == &kHeadersClass) {
    HeaderRecord* record = nullptr;
    auto slot = JS::GetReservedSlot(object, 0);
    if (slot.isUndefined()) return Error(cx, "headers requires a Headers object");
    record = static_cast<HeaderRecord*>(slot.toPrivate());
    for (size_t index = 0;; ++index) {
      std::string name, field;
      bool found = false;
      if (!Report(cx, Entry(record, index, &name, &field, &found))) return false;
      if (!found) return true;
      headers->emplace_back(std::move(name), std::move(field));
    }
  }
  bool array = false;
  if (!JS::IsArrayObject(cx, object, &array)) return false;
  if (array) {
    uint32_t length = 0;
    if (!JS::GetArrayLength(cx, object, &length) || length > 256)
      return Error(cx, "Too many headers");
    for (uint32_t index = 0; index < length; ++index) {
      JS::RootedValue row(cx);
      bool rowArray = false;
      uint32_t width = 0;
      JS::RootedObject pair(cx);
      JS::RootedValue name(cx), field(cx);
      std::string key, item;
      if (!JS_GetElement(cx, object, index, &row) || !row.isObject())
        return Error(cx, "Invalid header");
      pair = &row.toObject();
      if (!JS::IsArrayObject(cx, pair, &rowArray) || !rowArray ||
          !JS::GetArrayLength(cx, pair, &width) || width != 2 ||
          !JS_GetElement(cx, pair, 0, &name) || !JS_GetElement(cx, pair, 1, &field) ||
          !Convert(cx, name, &key) || !Convert(cx, field, &item)) return Error(cx, "Invalid header");
      headers->emplace_back(std::move(key), std::move(item));
    }
    return true;
  }
  JS::RootedIdVector keys(cx);
  if (!js::GetPropertyKeys(cx, object, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &keys))
    return false;
  for (size_t index = 0; index < keys.length(); ++index) {
    JS::RootedId key(cx, keys[index]);
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> descriptor(cx);
    if (!JS_GetOwnPropertyDescriptorById(cx, object, key, &descriptor)) return false;
    if (descriptor.isNothing() || !descriptor->enumerable()) continue;
    JS::RootedValue name(cx), field(cx);
    std::string converted;
    if (!JS_IdToValue(cx, key, &name) || !Convert(cx, name, &converted)) return false;
    std::string header(std::move(converted));
    if (!JS_GetPropertyById(cx, object, key, &field) || !Convert(cx, field, &converted)) return false;
    headers->emplace_back(std::move(header), std::move(converted));
  }
  return true;
}
bool PageBaseURL(State& s, std::string* base) {
  JS::RootedValue location(s.cx), href(s.cx);
  if (!JS_GetProperty(s.cx, s.page, "location", &location) || !location.isObject()) return false;
  JS::RootedObject object(s.cx, &location.toObject());
  return JS_GetProperty(s.cx, object, "href", &href) && Convert(s.cx, href, base);
}
bool ConvertInit(JSContext* cx, JS::HandleValue input, HeaderDraft* draft) {
  if (!input.isObject()) return Error(cx, "Headers init must be an object");
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(input, JS::ForOfIterator::AllowNonIterable)) return false;
  if (iterator.valueIsIterable()) return Sequence(cx, iterator, draft);
  JS::RootedObject object(cx, &input.toObject());
  return Record(cx, object, draft);
}
}
