// FileReader. The reader is a real EventTarget subclass: its prototype chains to
// EventTarget.prototype, so addEventListener and the on<type> handler attributes
// travel the interaction tree instead of a second listener store. Only what the
// interface owns lives in slots: readyState, result, error, the registered handler
// callbacks and the generation that invalidates a superseded read.
#include "web_platform/state.h"

#include "interaction/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs::web_platform {
const JSClass kReaderClass = {"FileReader", JSCLASS_HAS_RESERVED_SLOTS(kReaderSlotCount)};

namespace {
// The event type behind each on<type> handler attribute, in slot order.
constexpr const char* kHandlerTypes[] = {"loadstart", "progress", "load",
                                         "abort", "error", "loadend"};

bool HandlerMap(JSContext* cx, JS::HandleObject self, JS::MutableHandleObject map) {
  JS::RootedValue handlers(cx, JS::GetReservedSlot(self, kReaderHandlersSlot));
  if (!handlers.isObject()) return WebKit::Fail(cx, "FileReader handlers are not initialized");
  map.set(&handlers.toObject());
  return true;
}

// Calls self[name](...arguments) for the inherited EventTarget members, so a
// handler attribute registers on the one listener tree.
bool CallSelf(State& state, JS::HandleObject self, const char* name,
              JS::HandleValueArray arguments) {
  JSContext* cx = state.cx;
  JS::RootedValue method(cx), receiver(cx, JS::ObjectValue(*self)), result(cx);
  if (!JS_GetProperty(cx, self, name, &method) || !method.isObject() ||
      !JS::IsCallable(&method.toObject()))
    return WebKit::Fail(cx, "FileReader requires the EventTarget interface");
  return JS::Call(cx, receiver, method, arguments, &result);
}

bool Callback(JS::HandleValue value) {
  return value.isObject() && JS::IsCallable(&value.toObject());
}
}  // namespace

bool ReaderBrand(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return WebKit::Check(cx, self, &kReaderClass, out,
                       "FileReader member called on an incompatible receiver");
}

// The one error factory: the object comes from the page realm's DOMException
// interface, so the reader does not own a second error type.
bool MakeException(State& state, const char* name, const char* message,
                   JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  JS::RootedValue constructor(cx);
  if (!JS_GetProperty(cx, state.kit.page, "DOMException", &constructor) ||
      !constructor.isObject())
    return WebKit::Fail(cx, "FileReader requires the page DOMException");
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message));
  JS::RootedString label(cx, JS_NewStringCopyZ(cx, name));
  if (!text || !label) return false;
  JS::RootedValueArray<2> arguments(cx);
  arguments[0].setString(text);
  arguments[1].setString(label);
  JS::RootedObject error(cx);
  if (!JS::Construct(cx, constructor, arguments, &error)) return false;
  result.setObject(*error);
  return true;
}

bool FileReaderConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  NativeConstructorTrace trace(cx, args);
  if (!state.events) return WebKit::Fail(cx, "FileReader event owner is not installed");
  JS::RootedObject prototype(cx);
  if (!state.kit.ConstructProto(args, state.reader.prototype, &prototype)) return false;
  JS::RootedObject object(cx, state.kit.New(&kReaderClass, prototype));
  JS::RootedObject handlers(cx, JS_NewPlainObject(cx));
  if (!object || !handlers) return false;
  JS::SetReservedSlot(object, kReaderStateSlot, JS::Int32Value(kReaderReadyEmpty));
  JS::SetReservedSlot(object, kReaderResultSlot, JS::NullValue());
  JS::SetReservedSlot(object, kReaderErrorSlot, JS::NullValue());
  JS::SetReservedSlot(object, kReaderHandlersSlot, JS::ObjectValue(*handlers));
  JS::SetReservedSlot(object, kReaderGenerationSlot, JS::Int32Value(0));
  if (!state.events->Attach(object)) return false;
  args.rval().setObject(*object);
  return trace.Finish(true);
}

bool FileReaderGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!ReaderBrand(cx, args.thisv(), &self)) return false;
  switch (ReaderField(WebKit::Slot(args))) {
    case kReaderReadyStateField:
      args.rval().set(JS::GetReservedSlot(self, kReaderStateSlot));
      return true;
    case kReaderResultField:
      args.rval().set(JS::GetReservedSlot(self, kReaderResultSlot));
      return true;
    default:
      args.rval().set(JS::GetReservedSlot(self, kReaderErrorSlot));
      return true;
  }
}

bool ReaderHandlerGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx), map(cx);
  if (!ReaderBrand(cx, args.thisv(), &self) || !HandlerMap(cx, self, &map)) return false;
  JS::RootedValue current(cx);
  if (!JS_GetProperty(cx, map, kHandlerTypes[WebKit::Slot(args)], &current)) return false;
  if (Callback(current)) args.rval().set(current);
  else args.rval().setNull();
  return true;
}

// A handler attribute is an accessor over the one listener tree: assigning a
// callable swaps the previous registration, assigning null removes it.
bool ReaderHandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx), map(cx);
  if (!ReaderBrand(cx, args.thisv(), &self) || !HandlerMap(cx, self, &map)) return false;
  const char* type = kHandlerTypes[WebKit::Slot(args)];
  JS::RootedValue previous(cx);
  if (!JS_GetProperty(cx, map, type, &previous)) return false;
  JS::RootedValue desired(cx, args.get(0));
  if (Callback(previous)) {
    JS::RootedValueArray<2> remove(cx);
    JS::RootedString name(cx, JS_NewStringCopyZ(cx, type));
    if (!name) return false;
    remove[0].setString(name);
    remove[1].set(previous);
    if (!CallSelf(state, self, "removeEventListener", remove)) return false;
  }
  if (Callback(desired)) {
    JS::RootedValueArray<2> add(cx);
    JS::RootedString name(cx, JS_NewStringCopyZ(cx, type));
    if (!name) return false;
    add[0].setString(name);
    add[1].set(desired);
    if (!CallSelf(state, self, "addEventListener", add)) return false;
  } else {
    desired.setNull();
  }
  if (!JS_SetProperty(cx, map, type, desired)) return false;
  args.rval().setUndefined();
  return true;
}
}  // namespace ruyijs::web_platform
