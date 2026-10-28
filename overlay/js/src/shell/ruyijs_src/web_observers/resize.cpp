// ResizeObserver, ResizeObserverEntry and ResizeObserverSize. The interface
// shape, observe() options and error classes follow the Firefox donor 157
// (dom/webidl/ResizeObserver.webidl and the generated ResizeObserverBinding.cpp):
// three methods, no takeRecords, and [Frozen, Cached, Pure] size sequences.
// There is no layout engine, so every element box is the deterministic zero the
// D60 deviation records; the sequences are frozen arrays holding one branded
// ResizeObserverSize of inlineSize/blockSize zero.
#include "runtime/web_kit.h"
#include "web_observers/internal.h"
#include "web_observers/realm.h"
#include "document_model/realm.h"
#include "runtime/exception_capture.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/StableStringChars.h"
#include "js/String.h"
#include "jsfriendapi.h"
#include <algorithm>
#include <new>

namespace ruyijs::web_observers {
const JSClass kResizeClass = {"ResizeObserver", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kResizeEntryClass = {"ResizeObserverEntry", JSCLASS_HAS_RESERVED_SLOTS(5)};
const JSClass kResizeSizeClass = {"ResizeObserverSize", JSCLASS_HAS_RESERVED_SLOTS(2)};

namespace {
bool ASCIIEqual(const DomString& text, const char* ascii) {
  const size_t length = std::char_traits<char>::length(ascii);
  if (text.size() != length) return false;
  for (size_t index = 0; index < length; ++index)
    if (text[index] != char16_t(static_cast<unsigned char>(ascii[index])))
      return false;
  return true;
}

// ResizeObserverBoxOptions. WebIDL enum conversion stringifies and compares
// case-sensitively (binding_detail::FindEnumStringIndex<true>), and throws a
// TypeError on any other word.
bool Box(JSContext* cx, State& state, JS::HandleValue value, DomString* box) {
  if (value.isUndefined()) {
    *box = u"content-box";
    return true;
  }
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  JS::AutoStableStringChars chars(cx);
  if (!chars.initTwoByte(cx, text)) return false;
  DomString word(chars.twoByteChars(), chars.length());
  if (ASCIIEqual(word, "border-box") || ASCIIEqual(word, "content-box") ||
      ASCIIEqual(word, "device-pixel-content-box")) {
    *box = std::move(word);
    return true;
  }
  return state.document->ThrowTypeError(
      "ResizeObserver.observe box is not a valid ResizeObserverBoxOptions value");
}

Resize* Receiver(JSContext* cx, JS::CallArgs& args) {
  if (!args.thisv().isObject() || JS::GetClass(&args.thisv().toObject()) != &kResizeClass) {
    WebKit::FailBrand(cx, args, "ResizeObserver");
    return nullptr;
  }
  return static_cast<Resize*>(JS::GetReservedSlot(&args.thisv().toObject(), 0).toPrivate());
}
}  // namespace
bool ResizeConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state) { JS_ReportErrorASCII(cx, "Illegal constructor."); return false; }
  if (!args.isConstructing()) return state->document->ThrowTypeError("Illegal constructor.");
  if (!args.get(0).isObject() || !JS::IsCallable(&args.get(0).toObject()))
    return state->document->ThrowTypeError(
        "ResizeObserver requires a callable resize callback");
  JS::RootedObject callback(cx, &args.get(0).toObject());
  JS::RootedObject object(
      cx, JS_NewObjectWithGivenProto(cx, &kResizeClass, state->resizePrototype));
  if (!object) return false;
  Resize* observer = state->NewResize(object, callback);
  if (!observer) return false;
  JS::SetReservedSlot(object, 0, JS::PrivateValue(observer));
  args.rval().setObject(*object);
  return state->helpers->Object("ResizeObserver", object) && trace.Finish(true);
}

bool ResizeEntryConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state) { JS_ReportErrorASCII(cx, "Illegal constructor."); return false; }
  return state->document->ThrowTypeError("Illegal constructor.");
}

bool ResizeSizeConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state) { JS_ReportErrorASCII(cx, "Illegal constructor."); return false; }
  return state->document->ThrowTypeError("Illegal constructor.");
}

namespace {
bool SlotMember(JSContext* cx, JS::CallArgs& args, const JSClass* klass, const char* iface) {
  if (!args.thisv().isObject() || JS::GetClass(&args.thisv().toObject()) != klass) {
    auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
    if (state) {
      const std::string message = std::string(iface) + " member called on an incompatible receiver";
      return state->document->ThrowTypeError(message.c_str());
    }
    JS_ReportErrorASCII(cx, "observer member called on an incompatible receiver");
    return false;
  }
  int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  args.rval().set(JS::GetReservedSlot(&args.thisv().toObject(), size_t(slot)));
  return true;
}
}  // namespace

bool ResizeEntryMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return SlotMember(cx, args, &kResizeEntryClass, "ResizeObserverEntry");
}

bool ResizeSizeMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return SlotMember(cx, args, &kResizeSizeClass, "ResizeObserverSize");
}

namespace {
// One frozen [Frozen, Cached, Pure] size sequence holding a single branded size.
bool Sizes(State& state, JS::MutableHandleObject result) {
  JS::RootedObject array(state.cx, JS::NewArrayObject(state.cx, 1));
  JS::RootedObject size(state.cx);
  if (!array || !state.Size(0, 0, &size)) return false;
  JS::RootedValue item(state.cx, JS::ObjectValue(*size));
  if (!JS_DefineElement(state.cx, array, 0, item, JSPROP_ENUMERATE) ||
      !JS_FreezeObject(state.cx, array)) return false;
  result.set(array);
  return true;
}

bool BuildEntries(State& state, std::vector<NodeKey>* queued, JS::MutableHandleObject result) {
  JS::RootedObject array(state.cx, JS::NewArrayObject(state.cx, queued->size()));
  if (!array) return false;
  uint32_t index = 0;
  for (const NodeKey& key : *queued) {
    JS::RootedObject target(state.cx);
    if (!state.Resolve(key, &target)) return false;
    if (!target) continue;
    JS::RootedObject entry(state.cx);
    if (!state.BuildResizeEntry(target, &entry)) return false;
    JS::RootedValue item(state.cx, JS::ObjectValue(*entry));
    if (!JS_DefineElement(state.cx, array, index++, item, JSPROP_ENUMERATE)) return false;
  }
  JS::RootedValue length(state.cx, JS::NumberValue(index));
  if (!JS_SetProperty(state.cx, array, "length", length)) return false;
  result.set(array);
  return true;
}
}  // namespace

bool ResizeObserve(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Resize* observer = Receiver(cx, args);
  if (!observer) return false;
  State& state = *observer->owner;
  if (!state.IsElement(args.get(0)))
    return state.document->ThrowTypeError(
        "Argument 1 of ResizeObserver.observe does not implement interface Element");
  NodeKey target;
  if (!state.document->NodeIdentity(args.get(0), &target)) return false;
  DomString box;
  if (!args.get(1).isUndefined()) {
    if (!args.get(1).isObject())
      return state.document->ThrowTypeError("ResizeObserver options is not an object");
    JS::RootedObject options(cx, &args.get(1).toObject());
    JS::RootedValue member(cx);
    if (!JS_GetProperty(cx, options, "box", &member) || !Box(cx, state, member, &box))
      return false;
  } else {
    box = u"content-box";
  }
  args.rval().setUndefined();
  for (ResizeObservation& observation : observer->targets) {
    if (observation.target.document == target.document &&
        observation.target.node == target.node) {
      observation.box = std::move(box);  // observe() replaces the box, queues nothing.
      return true;
    }
  }
  observer->targets.push_back(ResizeObservation{target, std::move(box)});
  observer->queued.push_back(target);
  if (!observer->scheduled) {
    JS::AutoSaveExceptionState saved(cx);
    JS::RootedObject deliver(cx, state.resizeDeliver);
    if (state.Queue(deliver)) observer->scheduled = true;
    saved.restore();
  }
  return true;
}

bool ResizeUnobserve(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Resize* observer = Receiver(cx, args);
  if (!observer) return false;
  State& state = *observer->owner;
  if (!state.IsElement(args.get(0)))
    return state.document->ThrowTypeError(
        "Argument 1 of ResizeObserver.unobserve does not implement interface Element");
  NodeKey target;
  if (!state.document->NodeIdentity(args.get(0), &target)) return false;
  args.rval().setUndefined();
  auto target_matches = [&](const NodeKey& key) {
    return key.document == target.document && key.node == target.node;
  };
  observer->targets.erase(
      std::remove_if(observer->targets.begin(), observer->targets.end(),
                     [&](const ResizeObservation& observation) {
                       return target_matches(observation.target);
                     }),
      observer->targets.end());
  observer->queued.erase(std::remove_if(observer->queued.begin(), observer->queued.end(),
                                        target_matches), observer->queued.end());
  return true;
}

bool ResizeDisconnect(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Resize* observer = Receiver(cx, args);
  if (!observer) return false;
  observer->targets.clear();
  observer->queued.clear();
  observer->scheduled = false;
  args.rval().setUndefined();
  return true;
}

bool ResizeDeliver(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  args.rval().setUndefined();
  if (state) state->DeliverResizes();
  return true;
}

Resize* State::NewResize(JS::HandleObject object, JS::HandleObject callback) {
  if (resizeCount >= kObserverLimit) {
    document->ThrowTypeError("ResizeObserver limit exceeded");
    return nullptr;
  }
  std::unique_ptr<Resize> created(new (std::nothrow) Resize(cx, object, callback));
  if (!created) { JS_ReportOutOfMemory(cx); return nullptr; }
  created->owner = this;
  ++resizeCount;
  Resize* added = created.get();
  if (!resizes) {
    resizes = std::move(created);
  } else {
    Resize* tail = resizes.get();
    while (tail->next) tail = tail->next.get();
    tail->next = std::move(created);
  }
  return added;
}

bool State::BuildResizeEntry(JS::HandleObject target, JS::MutableHandleObject result) {
  JS::RootedObject rect(cx), border(cx), content(cx), device(cx);
  if (!Rect(0, 0, 0, 0, &rect) || !Sizes(*this, &border) || !Sizes(*this, &content) ||
      !Sizes(*this, &device)) return false;
  JS::RootedObject object(
      cx, JS_NewObjectWithGivenProto(cx, &kResizeEntryClass, resizeEntryPrototype));
  if (!object) return false;
  JS::SetReservedSlot(object, 0, JS::ObjectValue(*target));
  JS::SetReservedSlot(object, 1, JS::ObjectValue(*rect));
  JS::SetReservedSlot(object, 2, JS::ObjectValue(*border));
  JS::SetReservedSlot(object, 3, JS::ObjectValue(*content));
  JS::SetReservedSlot(object, 4, JS::ObjectValue(*device));
  if (!helpers->Object("ResizeObserverEntry", object)) return false;
  result.set(object);
  return true;
}

void State::DeliverResizes() {
  for (Resize* observer = resizes.get(); observer; observer = observer->next.get()) {
    if (!observer->scheduled) continue;
    observer->scheduled = false;
    if (observer->queued.empty()) continue;
    std::vector<NodeKey> batch;
    batch.swap(observer->queued);
    JS::RootedObject entries(cx);
    if (!BuildEntries(*this, &batch, &entries)) {
      CaptureOperationFailure(cx);
      JS_ClearPendingException(cx);
      continue;
    }
    JS::RootedObject self(cx, observer->object);
    JS::RootedValue callback(cx, JS::ObjectValue(*observer->callback));
    JS::RootedValueArray<2> call(cx);
    call[0].setObject(*entries);
    call[1].setObject(*self);
    JS::RootedValue ignored(cx);
    if (!JS::Call(cx, JS::UndefinedHandleValue, callback, call, &ignored)) {
      CaptureOperationFailure(cx);
      JS_ClearPendingException(cx);
    }
  }
}

bool InstallResize(State& s) {
  JS::RootedObject size_prototype(s.cx), size_constructor(s.cx);
  if (!NewInterface(s, "ResizeObserverSize", 0, ResizeSizeConstructor, &size_prototype,
                    &size_constructor)) return false;
  static const char* const size_members[] = {"inlineSize", "blockSize"};
  for (int32_t slot = 0; slot < 2; ++slot)
    if (!Getter(s, "ResizeObserverSize", size_prototype, size_members[slot],
                ResizeSizeMember, slot)) return false;
  if (!LinkInterface(s, size_prototype, size_constructor) ||
      !JS_DefineProperty(s.cx, s.page, "ResizeObserverSize", size_constructor, 0)) return false;
  s.resizeSizePrototype = size_prototype.get();
  JS::RootedObject entry_prototype(s.cx), entry_constructor(s.cx);
  if (!NewInterface(s, "ResizeObserverEntry", 0, ResizeEntryConstructor, &entry_prototype,
                    &entry_constructor)) return false;
  static const char* const entry_members[] = {
      "target", "contentRect", "borderBoxSize", "contentBoxSize",
      "devicePixelContentBoxSize"};
  for (int32_t slot = 0; slot < 5; ++slot)
    if (!Getter(s, "ResizeObserverEntry", entry_prototype, entry_members[slot],
                ResizeEntryMember, slot)) return false;
  if (!LinkInterface(s, entry_prototype, entry_constructor) ||
      !JS_DefineProperty(s.cx, s.page, "ResizeObserverEntry", entry_constructor, 0)) return false;
  s.resizeEntryPrototype = entry_prototype.get();
  JS::RootedObject prototype(s.cx), constructor(s.cx);
  if (!NewInterface(s, "ResizeObserver", 1, ResizeConstructor, &prototype, &constructor))
    return false;
  static const struct { const char* name; JSNative native; unsigned length; } methods[] = {
      {"observe", ResizeObserve, 1}, {"unobserve", ResizeUnobserve, 1},
      {"disconnect", ResizeDisconnect, 0}};
  for (const auto& method : methods)
    if (!Method(s, "ResizeObserver", prototype, method.name, method.native, method.length))
      return false;
  if (!LinkInterface(s, prototype, constructor) ||
      !JS_DefineProperty(s.cx, s.page, "ResizeObserver", constructor, 0)) return false;
  s.resizePrototype = prototype.get();
  JS::RootedObject deliver(s.cx, Function(s, ResizeDeliver, "deliverResizes", 0, 0));
  if (!deliver) return false;
  s.resizeDeliver = deliver.get();
  return true;
}
}  // namespace ruyijs::web_observers
