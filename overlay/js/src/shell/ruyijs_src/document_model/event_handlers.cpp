// Element and Document event handler IDL attributes (GlobalEventHandlers),
// docs/owners/DOCUMENT_MODEL.md. A handler is one listener on the Interaction
// owner's single listener tree: the type's invoker is added when the handler
// first becomes non-null, removed when it becomes null, and a replacement keeps
// its position, as Firefox 157 does (tests/fixtures/element_event_handlers_firefox.json).
// Values live in the node wrapper's existing handler slot.
#include "document_model/state.h"
#include "document_model/event_handler_names.h"
#include "document_model/body_frameset_handlers.h"
#include "runtime/exception_capture.h"
#include "interaction/realm.h"
#include "js/Exception.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cstring>
#include <string>

namespace ruyijs::document_model {
namespace {
constexpr unsigned kHandlerSlot = 2;
enum Kind { HTML, SVG, MathML, Doc, Elem, KindCount };
constexpr const char* kInterfaces[] = {"HTMLElement", "SVGElement", "MathMLElement", "Document", "Element"};
constexpr int32_t kStride = 1024;
// Type ids index the Document list (it holds the global list), then Element's pair.
const char* HandlerName(int32_t id) {
  return size_t(id) < kDocumentEventHandlerCount ? kDocumentEventHandlerNames[id]
                                                 : kElementEventHandlerNames[id - kDocumentEventHandlerCount];
}
int32_t HandlerId(const char* name) {
  for (size_t i = 0; i < kDocumentEventHandlerCount; ++i)
    if (!std::strcmp(kDocumentEventHandlerNames[i], name)) return int32_t(i);
  for (size_t i = 0; i < kElementEventHandlerCount; ++i)
    if (!std::strcmp(kElementEventHandlerNames[i], name)) return int32_t(kDocumentEventHandlerCount + i);
  return -1;
}
bool Brand(Kind kind, const NodeSnapshot& node) {
  if (kind == Doc) return node.kind == NodeKind::Document;
  if (node.kind != NodeKind::Element) return false;
  if (kind == HTML) return node.is_html_element;
  if (kind == SVG) return node.namespace_uri && *node.namespace_uri == u"http://www.w3.org/2000/svg";
  if (kind == MathML) return node.namespace_uri && *node.namespace_uri == u"http://www.w3.org/1998/Math/MathML";
  return true;
}
Wrapper* Receiver(JSContext* cx, const JS::CallArgs& args, bool setter) {
  const int32_t slot = FunctionSlot(args);
  const Kind kind = Kind(slot / kStride);
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  Wrapper* entry = object && JS::GetClass(object) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  NodeSnapshot node;
  if (entry && !entry->dead && entry->owner &&
      entry->owner->Check(entry->owner->model.Inspect(entry->key, &node)) && Brand(kind, node))
    return entry;
  if (JS_IsExceptionPending(cx)) return nullptr;
  const std::string message = std::string("'") + (setter ? "set " : "get ") +
      HandlerName(slot % kStride) + "' called on an object that does not implement interface " +
      kInterfaces[kind] + ".";
  TypeError(cx, message.c_str());
  return nullptr;
}
bool Store(JSContext* cx, JS::HandleObject wrapper, JS::MutableHandleObject store) {
  JS::Value slot = JS::GetReservedSlot(wrapper, kHandlerSlot);
  if (slot.isObject()) { store.set(&slot.toObject()); return true; }
  store.set(JS_NewObjectWithGivenProto(cx, nullptr, nullptr));
  if (!store) return false;
  JS::SetReservedSlot(wrapper, kHandlerSlot, JS::ObjectValue(*store));
  return true;
}
bool Invoker(State& state, int32_t id, JS::MutableHandleObject result);
bool Listen(State& state, JS::HandleObject wrapper, int32_t id, bool adding) {
  JSContext* cx = state.cx;
  JS::RootedObject invoker(cx);
  JS::RootedString type(cx, JS_NewStringCopyZ(cx, HandlerName(id) + 2));
  if (!type || !Invoker(state, id, &invoker)) return false;
  JS::RootedValue method(cx, JS::ObjectValue(adding ? *state.handlerAdd : *state.handlerRemove));
  JS::RootedValue receiver(cx, JS::ObjectValue(*wrapper)), ignored(cx);
  JS::RootedValueArray<2> arguments(cx);
  arguments[0].setString(type);
  arguments[1].setObject(*invoker);
  return JS_WrapValue(cx, &receiver) && JS::Call(cx, receiver, method, arguments, &ignored);
}
bool Invoke(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t id = FunctionSlot(args);
  args.rval().setUndefined();
  JS::RootedObject target(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!target || JS::GetClass(target) != &kNodeClass || !args.get(0).isObject()) return true;
  auto* entry = static_cast<Wrapper*>(JS::GetReservedSlot(target, 0).toPrivate());
  if (!entry || entry->dead || !entry->owner) return true;
  State& state = *entry->owner;
  JSAutoRealm realm(cx, state.page);
  JS::RootedValue handler(cx), event(cx, args[0]), result(cx);
  if (!ReadEventHandler(state, target, HandlerName(id), &handler)) return false;
  // A non-callable object handler does nothing, as in Firefox.
  if (!handler.isObject() || !JS::IsCallable(&handler.toObject())) return true;
  JS::RootedValue receiver(cx, JS::ObjectValue(*target));
  if (!JS_WrapValue(cx, &event) ||
      !JS::Call(cx, receiver, handler, JS::HandleValueArray(event), &result)) return false;
  if (!result.isFalse()) return true;
  JS::RootedValue prevent(cx, JS::ObjectValue(*state.handlerPrevent)), ignored(cx);
  return JS::Call(cx, event, prevent, JS::HandleValueArray::empty(), &ignored);
}
bool Invoker(State& state, int32_t id, JS::MutableHandleObject result) {
  JSContext* cx = state.cx;
  JS::RootedObject invokers(cx, state.handlerInvokers);
  JS::RootedValue cached(cx);
  if (!JS_GetElement(cx, invokers, uint32_t(id), &cached)) return false;
  if (cached.isObject()) { result.set(&cached.toObject()); return true; }
  result.set(state.Function(Invoke, HandlerName(id), 1, id));
  if (!result) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*result));
  return JS_SetElement(cx, invokers, uint32_t(id), value);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = Receiver(cx, args, false);
  if (!entry) return false;
  const char* name = HandlerName(FunctionSlot(args) % kStride);
  entry->owner->Observe(kInterfaces[FunctionSlot(args) / kStride], name, int(ApiAccess::Read));
  NodeSnapshot node;
  if (!entry->owner->Check(entry->owner->model.Inspect(entry->key, &node))) return false;
  // body/frameset reflect their window-forwarded GlobalEventHandlers members
  // (onblur/onerror/onfocus/onload/onresize/onscroll) to the Window, so the
  // value comes from the Window owner's accessor, not the element's store.
  if (IsBodyFramesetInterface(node.interface_name) && IsBodyFramesetWindowHandler(name)) {
    State& state = *entry->owner;
    if (entry->key.document != state.main.document) { args.rval().setNull(); return true; }
    {
      JSAutoRealm realm(cx, state.page);
      if (!JS_GetProperty(cx, state.page, name, args.rval())) return false;
    }
    return JS_WrapValue(cx, args.rval());
  }
  {
    JSAutoRealm realm(cx, entry->owner->page);
    JS::RootedObject wrapper(cx, entry->object);
    if (!ReadEventHandler(*entry->owner, wrapper, name, args.rval())) return false;
  }
  return JS_WrapValue(cx, args.rval());
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = Receiver(cx, args, true);
  if (!entry) return false;
  const char* name = HandlerName(FunctionSlot(args) % kStride);
  entry->owner->Observe(kInterfaces[FunctionSlot(args) / kStride], name, int(ApiAccess::Write));
  // [LegacyTreatNonObjectAsNull]: any object is kept, everything else is null.
  JS::RootedValue value(cx, args.get(0).isObject() ? args.get(0) : JS::NullValue());
  NodeSnapshot node;
  if (!entry->owner->Check(entry->owner->model.Inspect(entry->key, &node))) return false;
  if (IsBodyFramesetInterface(node.interface_name) && IsBodyFramesetWindowHandler(name)) {
    State& state = *entry->owner;
    if (entry->key.document != state.main.document) { args.rval().setUndefined(); return true; }
    {
      JSAutoRealm realm(cx, state.page);
      if (!JS_WrapValue(cx, &value) || !JS_SetProperty(cx, state.page, name, value)) return false;
    }
    args.rval().setUndefined(); return true;
  }
  JSAutoRealm realm(cx, entry->owner->page);
  JS::RootedObject wrapper(cx, entry->object);
  if (!JS_WrapValue(cx, &value) || !WriteEventHandler(*entry->owner, wrapper, name, value)) return false;
  args.rval().setUndefined();
  return true;
}
}  // namespace
bool WriteEventHandler(State& state, JS::HandleObject wrapper, const char* name, JS::HandleValue value) {
  JSContext* cx = state.cx;
  const int32_t id = HandlerId(name);
  if (id < 0) return TypeError(cx, "Unknown event handler");
  JS::RootedObject store(cx);
  JS::RootedValue previous(cx);
  if (!Store(cx, wrapper, &store) || !JS_GetProperty(cx, store, name, &previous)) return false;
  // An uncompiled content attribute (a string) is an active handler too.
  const bool was = previous.isObject() || previous.isString();
  const bool now = value.isObject() || value.isString();
  if (!was && now && !Listen(state, wrapper, id, true)) return false;
  if (was && !now && !Listen(state, wrapper, id, false)) return false;
  return JS_SetProperty(cx, store, name, value);
}
bool ReadEventHandler(State& state, JS::HandleObject wrapper, const char* name, JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  JS::RootedObject store(cx);
  if (!Store(cx, wrapper, &store) || !JS_GetProperty(cx, store, name, result)) return false;
  if (result.isString()) {
    JS::RootedString source(cx, result.toString());
    JS::RootedObject compiled(cx);
    // A body that does not compile is reported like an uncaught script error
    // and leaves no handler; the getter itself does not throw.
    if (!CompileEventHandler(state, wrapper, name, source, &compiled)) {
      if (JS_IsThrowingOutOfMemory(cx)) return false;
      CaptureOperationFailure(cx);
      JS_ClearPendingException(cx);
      result.setNull();
      return WriteEventHandler(state, wrapper, name, result);
    }
    result.setObject(*compiled);
    if (!JS_SetProperty(cx, store, name, result)) return false;
  }
  if (result.isUndefined()) result.setNull();
  return true;
}
bool HasEventHandler(JS::HandleObject wrapper, const char* name, JSContext* cx, bool* present) {
  *present = false;
  JS::Value slot = JS::GetReservedSlot(wrapper, kHandlerSlot);
  if (!slot.isObject()) return true;
  JS::RootedObject store(cx, &slot.toObject());
  return JS_HasOwnProperty(cx, store, name, present);
}
bool InstallEventHandlers(State& state) {
  JSContext* cx = state.cx;
  JS::RootedValue value(cx);
  JS::RootedObject target(cx, state.interaction.prototype());
  // The original natives, saved before page script can replace them.
  if (!JS_GetProperty(cx, target, "addEventListener", &value) || !value.isObject()) return false;
  state.handlerAdd = &value.toObject();
  if (!JS_GetProperty(cx, target, "removeEventListener", &value) || !value.isObject()) return false;
  state.handlerRemove = &value.toObject();
  JS::RootedObject constructor(cx);
  if (!JS_GetProperty(cx, state.page, "Event", &value) || !value.isObject()) return false;
  constructor = &value.toObject();
  if (!JS_GetProperty(cx, constructor, "prototype", &value) || !value.isObject()) return false;
  constructor = &value.toObject();
  if (!JS_GetProperty(cx, constructor, "preventDefault", &value) || !value.isObject()) return false;
  state.handlerPrevent = &value.toObject();
  state.handlerInvokers = JS_NewObjectWithGivenProto(cx, nullptr, nullptr);
  if (!state.handlerInvokers) return false;
  for (int kind = HTML; kind < KindCount; ++kind) {
    const char* const* names = kind == Doc ? kDocumentEventHandlerNames
        : kind == Elem ? kElementEventHandlerNames : kGlobalEventHandlerNames;
    const size_t count = kind == Doc ? kDocumentEventHandlerCount
        : kind == Elem ? kElementEventHandlerCount : kGlobalEventHandlerCount;
    for (size_t i = 0; i < count; ++i) {
      if (!state.Accessor(kInterfaces[kind], names[i], Get, kind * kStride + HandlerId(names[i]), Set))
        return false;
      // EventHandler.webidl: [SecureContext] attribute EventHandler onpointerrawupdate.
      if (!std::strcmp(names[i], "onpointerrawupdate")) {
        JS::RootedObject prototype(cx, state.Find(kInterfaces[kind])->prototype);
        if (!state.secure.Add(prototype, names[i])) return false;
      }
    }
  }
  return true;
}
}  // namespace ruyijs::document_model
