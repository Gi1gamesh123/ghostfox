// IntersectionObserver and IntersectionObserverEntry. The interface shape,
// option parsing and error classes follow the Firefox donor 157
// (dom/webidl/IntersectionObserver.webidl, dom/base/DOMIntersectionObserver.cpp,
// and the generated IntersectionObserverBinding.cpp). There is no layout
// engine, so an entry carries the deterministic geometry the D59 deviation
// records: zero element rects, a viewport-sized rootBounds read from the window
// facade, isIntersecting false and intersectionRatio zero.
#include "web_observers/internal.h"
#include "web_observers/margin.h"
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
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/StableStringChars.h"
#include "js/String.h"
#include "jsfriendapi.h"
#include <algorithm>
#include <cmath>
#include <new>

namespace ruyijs::web_observers {
const JSClass kIntersectionClass = {"IntersectionObserver", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kIntersectionEntryClass = {"IntersectionObserverEntry",
                                         JSCLASS_HAS_RESERVED_SLOTS(7)};

namespace {
// A plain RangeError, like the donor's ThrowRangeError<MSG_THRESHOLD_RANGE_ERROR>.
bool RangeError(JSContext* cx, const char* message) {
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message));
  JS::RootedString file(cx, JS_NewStringCopyZ(cx, "ruyijs:observers"));
  JS::Rooted<mozilla::Maybe<JS::Value>> cause(cx, mozilla::Nothing());
  JS::RootedValue value(cx);
  if (!text || !file || !JS::CreateError(cx, JSEXN_RANGEERR, nullptr, file, 1,
                                         JS::ColumnNumberOneOrigin(), nullptr, text, cause,
                                         &value)) return false;
  JS_SetPendingException(cx, value);
  return false;
}

bool String(JSContext* cx, JS::HandleValue value, DomString* result) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  JS::AutoStableStringChars chars(cx);
  if (!chars.initTwoByte(cx, text)) return false;
  result->assign(chars.twoByteChars(), chars.length());
  return true;
}

bool Array(JSContext* cx, const std::vector<double>& values, JS::MutableHandleObject result) {
  JS::RootedObject array(cx, JS::NewArrayObject(cx, values.size()));
  if (!array) return false;
  for (uint32_t index = 0; index < values.size(); ++index) {
    JS::RootedValue item(cx, JS::NumberValue(values[index]));
    if (!JS_DefineElement(cx, array, index, item, JSPROP_ENUMERATE)) return false;
  }
  result.set(array);
  return true;
}

Intersection* Receiver(JSContext* cx, JS::CallArgs& args) {
  if (!args.thisv().isObject() || JS::GetClass(&args.thisv().toObject()) != &kIntersectionClass) {
    auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
    if (state)
      state->document->ThrowTypeError(
          "IntersectionObserver member called on an incompatible receiver");
    else
      JS_ReportErrorASCII(cx, "IntersectionObserver member called on an incompatible receiver");
    return nullptr;
  }
  return static_cast<Intersection*>(
      JS::GetReservedSlot(&args.thisv().toObject(), 0).toPrivate());
}

// threshold: (double or sequence<double>). The union conversion tries the
// iterable branch for objects first and the double branch otherwise; a
// non-finite double and a non-iterable object are both TypeErrors.
bool Thresholds(JSContext* cx, State& state, JS::HandleValue value,
                std::vector<double>* values) {
  bool sequence = false;
  JS::RootedValue member(cx, value);
  JS::ForOfIterator iterate(cx);
  if (member.isObject()) {
    if (!iterate.init(member, JS::ForOfIterator::AllowNonIterable)) return false;
    sequence = iterate.valueIsIterable();
  }
  if (sequence) {
    JS::RootedValue item(cx);
    while (true) {
      bool done = false;
      if (!iterate.next(&item, &done)) return false;
      if (done) break;
      double number = 0;
      if (!JS::ToNumber(cx, item, &number)) return false;
      if (!std::isfinite(number))
        return state.document->ThrowTypeError(
            "IntersectionObserver.threshold is not a finite floating-point value");
      if (values->size() >= 4096)
        return state.document->ThrowTypeError("threshold exceeds the product limit");
      values->push_back(number);
    }
  } else {
    double number = 0;
    if (!JS::ToNumber(cx, member, &number)) return false;
    if (!std::isfinite(number))
      return state.document->ThrowTypeError(
          "IntersectionObserver.threshold is not a finite floating-point value");
    values->push_back(number);
  }
  for (double threshold : *values)
    if (threshold < 0.0 || threshold > 1.0)
      return RangeError(cx, "IntersectionObserver.Threshold values must all be in the range [0, 1].");
  std::sort(values->begin(), values->end());
  if (values->empty()) values->push_back(0.0);
  return true;
}
}  // namespace
bool IntersectionConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state) { JS_ReportErrorASCII(cx, "Illegal constructor."); return false; }
  if (!args.isConstructing()) return state->document->ThrowTypeError("Illegal constructor.");
  if (!args.get(0).isObject() || !JS::IsCallable(&args.get(0).toObject()))
    return state->document->ThrowTypeError(
        "IntersectionObserver requires a callable intersection callback");
  JS::RootedObject callback(cx, &args.get(0).toObject());
  // Dictionary members are read in IDL order (root, rootMargin, scrollMargin,
  // threshold), so the first failure is the donor's first failure.
  NodeKey root;
  // The IDL dictionary default is the string "0px"; the donor parses it and
  // the getter re-serializes the parsed margin, so the default reads back as
  // the four-component form.
  DomString root_margin = SerializeMargin(Margin());
  DomString scroll_margin = SerializeMargin(Margin());
  std::vector<double> thresholds;
  if (!args.get(1).isUndefined()) {
    if (!args.get(1).isObject())
      return state->document->ThrowTypeError("IntersectionObserver options is not an object");
    JS::RootedObject options(cx, &args.get(1).toObject());
    JS::RootedValue member(cx);
    if (!JS_GetProperty(cx, options, "root", &member)) return false;
    if (!member.isNullOrUndefined()) {
      if (!state->IsElement(member) && !state->IsDocument(member))
        return state->document->ThrowTypeError(
            "IntersectionObserver.root must be an Element or Document");
      if (!state->document->NodeIdentity(member, &root)) return false;
    }
    if (!JS_GetProperty(cx, options, "rootMargin", &member)) return false;
    if (!member.isUndefined()) {
      DomString text;
      Margin margin;
      if (!String(cx, member, &text) || !ParseMargin(text, &margin))
        return state->document->ThrowException(
            "SyntaxError", "rootMargin must be specified in pixels or percent.");
      root_margin = SerializeMargin(margin);
    }
    if (!JS_GetProperty(cx, options, "scrollMargin", &member)) return false;
    if (!member.isUndefined()) {
      DomString text;
      Margin margin;
      if (!String(cx, member, &text) || !ParseMargin(text, &margin))
        return state->document->ThrowException(
            "SyntaxError", "scrollMargin must be specified in pixels or percent.");
      scroll_margin = SerializeMargin(margin);
    }
    if (!JS_GetProperty(cx, options, "threshold", &member)) return false;
    if (!member.isUndefined() && !Thresholds(cx, *state, member, &thresholds)) return false;
  }
  if (thresholds.empty()) thresholds.push_back(0.0);  // The dictionary default.
  JS::RootedObject object(
      cx, JS_NewObjectWithGivenProto(cx, &kIntersectionClass, state->intersectionPrototype));
  if (!object) return false;
  Intersection* observer = state->NewIntersection(object, callback);
  if (!observer) return false;
  JS::SetReservedSlot(object, 0, JS::PrivateValue(observer));
  observer->root = root;
  observer->rootMargin = std::move(root_margin);
  observer->scrollMargin = std::move(scroll_margin);
  observer->thresholds = std::move(thresholds);
  JS::RootedObject cached(cx);
  if (!Array(cx, observer->thresholds, &cached)) return false;
  observer->cachedThresholds = cached.get();
  args.rval().setObject(*object);
  return state->helpers->Object("IntersectionObserver", object) && trace.Finish(true);
}

bool IntersectionEntryConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state) { JS_ReportErrorASCII(cx, "Illegal constructor."); return false; }
  return state->document->ThrowTypeError("Illegal constructor.");
}

bool IntersectionMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Intersection* observer = Receiver(cx, args);
  if (!observer) return false;
  State& state = *observer->owner;
  int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  if (slot == 0) {
    JS::RootedObject root(cx);
    if (!state.Resolve(observer->root, &root)) return false;
    args.rval().set(root ? JS::ObjectValue(*root) : JS::NullValue());
    return true;
  }
  if (slot == 1 || slot == 2) {
    const DomString& text = slot == 1 ? observer->rootMargin : observer->scrollMargin;
    JS::RootedString value(cx, JS_NewUCStringCopyN(cx, text.data(), text.size()));
    if (!value) return false;
    args.rval().setString(value);
    return true;
  }
  args.rval().setObject(*observer->cachedThresholds);
  return true;
}

bool IntersectionEntryMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.thisv().isObject() ||
      JS::GetClass(&args.thisv().toObject()) != &kIntersectionEntryClass) {
    auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
    if (state)
      return state->document->ThrowTypeError(
          "IntersectionObserverEntry member called on an incompatible receiver");
    JS_ReportErrorASCII(cx, "IntersectionObserverEntry member called on an incompatible receiver");
    return false;
  }
  int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  args.rval().set(JS::GetReservedSlot(&args.thisv().toObject(), size_t(slot)));
  return true;
}
namespace {
// Builds the entry array the callback or takeRecords hands out. A target whose
// identity no longer resolves drops its slot, exactly like a vanished record
// node in the MutationObserver delivery path.
bool BuildEntries(State& state, std::vector<NodeKey>* queued, JS::MutableHandleObject result) {
  JS::RootedObject array(state.cx, JS::NewArrayObject(state.cx, queued->size()));
  if (!array) return false;
  uint32_t index = 0;
  for (const NodeKey& key : *queued) {
    JS::RootedObject target(state.cx);
    if (!state.Resolve(key, &target)) return false;
    if (!target) continue;
    JS::RootedObject entry(state.cx);
    if (!state.BuildIntersectionEntry(target, &entry)) return false;
    JS::RootedValue item(state.cx, JS::ObjectValue(*entry));
    if (!JS_DefineElement(state.cx, array, index++, item, JSPROP_ENUMERATE)) return false;
  }
  JS::RootedValue length(state.cx, JS::NumberValue(index));
  if (!JS_SetProperty(state.cx, array, "length", length)) return false;
  result.set(array);
  return true;
}
}  // namespace

bool IntersectionObserve(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Intersection* observer = Receiver(cx, args);
  if (!observer) return false;
  State& state = *observer->owner;
  if (!state.IsElement(args.get(0)))
    return state.document->ThrowTypeError(
        "Argument 1 of IntersectionObserver.observe does not implement interface Element");
  NodeKey target;
  if (!state.document->NodeIdentity(args.get(0), &target)) return false;
  args.rval().setUndefined();
  for (const NodeKey& existing : observer->targets)
    if (existing.document == target.document && existing.node == target.node) return true;
  observer->targets.push_back(target);
  observer->queued.push_back(target);
  if (!observer->scheduled) {
    // Queueing allocates. A failure is not the observe() failure: the entry
    // stays queued and takeRecords() can still hand it out.
    JS::AutoSaveExceptionState saved(cx);
    JS::RootedObject deliver(cx, state.intersectionDeliver);
    if (state.Queue(deliver)) observer->scheduled = true;
    saved.restore();
  }
  return true;
}

bool IntersectionUnobserve(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Intersection* observer = Receiver(cx, args);
  if (!observer) return false;
  State& state = *observer->owner;
  if (!state.IsElement(args.get(0)))
    return state.document->ThrowTypeError(
        "Argument 1 of IntersectionObserver.unobserve does not implement interface Element");
  NodeKey target;
  if (!state.document->NodeIdentity(args.get(0), &target)) return false;
  args.rval().setUndefined();
  auto target_matches = [&](const NodeKey& key) {
    return key.document == target.document && key.node == target.node;
  };
  observer->targets.erase(std::remove_if(observer->targets.begin(), observer->targets.end(),
                                         target_matches), observer->targets.end());
  observer->queued.erase(std::remove_if(observer->queued.begin(), observer->queued.end(),
                                        target_matches), observer->queued.end());
  return true;
}

bool IntersectionDisconnect(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Intersection* observer = Receiver(cx, args);
  if (!observer) return false;
  observer->targets.clear();
  observer->queued.clear();
  observer->scheduled = false;
  args.rval().setUndefined();
  return true;
}

bool IntersectionTakeRecords(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Intersection* observer = Receiver(cx, args);
  if (!observer) return false;
  std::vector<NodeKey> batch;
  batch.swap(observer->queued);
  observer->scheduled = false;  // The queued microtask now has nothing to send.
  JS::RootedObject entries(cx);
  if (!BuildEntries(*observer->owner, &batch, &entries)) return false;
  args.rval().setObject(*entries);
  return true;
}

bool IntersectionDeliver(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  args.rval().setUndefined();
  if (state) state->DeliverIntersections();
  return true;
}

Intersection* State::NewIntersection(JS::HandleObject object, JS::HandleObject callback) {
  if (intersectionCount >= kObserverLimit) {
    document->ThrowTypeError("IntersectionObserver limit exceeded");
    return nullptr;
  }
  std::unique_ptr<Intersection> created(new (std::nothrow) Intersection(cx, object, callback));
  if (!created) { JS_ReportOutOfMemory(cx); return nullptr; }
  created->owner = this;
  ++intersectionCount;
  Intersection* added = created.get();
  if (!intersections) {
    intersections = std::move(created);
  } else {
    Intersection* tail = intersections.get();
    while (tail->next) tail = tail->next.get();
    tail->next = std::move(created);
  }
  return added;
}

bool State::BuildIntersectionEntry(JS::HandleObject target, JS::MutableHandleObject result) {
  double width = 0, height = 0;
  JS::RootedObject bounding(cx), intersection(cx), root_bounds(cx);
  if (!Viewport(&width, &height) || !Rect(0, 0, 0, 0, &bounding) ||
      !Rect(0, 0, 0, 0, &intersection) || !Rect(0, 0, width, height, &root_bounds))
    return false;
  JS::RootedObject object(
      cx, JS_NewObjectWithGivenProto(cx, &kIntersectionEntryClass, intersectionEntryPrototype));
  if (!object) return false;
  JS::SetReservedSlot(object, 0, JS::NumberValue(Time()));
  JS::SetReservedSlot(object, 1, JS::ObjectValue(*root_bounds));
  JS::SetReservedSlot(object, 2, JS::ObjectValue(*bounding));
  JS::SetReservedSlot(object, 3, JS::ObjectValue(*intersection));
  JS::SetReservedSlot(object, 4, JS::BooleanValue(false));
  JS::SetReservedSlot(object, 5, JS::NumberValue(0));
  JS::SetReservedSlot(object, 6, JS::ObjectValue(*target));
  if (!helpers->Object("IntersectionObserverEntry", object)) return false;
  result.set(object);
  return true;
}

void State::DeliverIntersections() {
  for (Intersection* observer = intersections.get(); observer; observer = observer->next.get()) {
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
      // A throwing callback is reported like any other uncaught microtask
      // exception; it never stops the Context or the other observers.
      CaptureOperationFailure(cx);
      JS_ClearPendingException(cx);
    }
  }
}

bool InstallIntersection(State& s) {
  JS::RootedObject entry_prototype(s.cx), entry_constructor(s.cx);
  if (!NewInterface(s, "IntersectionObserverEntry", 0, IntersectionEntryConstructor,
                    &entry_prototype, &entry_constructor)) return false;
  static const char* const entry_members[] = {
      "time", "rootBounds", "boundingClientRect", "intersectionRect",
      "isIntersecting", "intersectionRatio", "target"};
  for (int32_t slot = 0; slot < 7; ++slot)
    if (!Getter(s, "IntersectionObserverEntry", entry_prototype, entry_members[slot],
                IntersectionEntryMember, slot)) return false;
  if (!LinkInterface(s, entry_prototype, entry_constructor) ||
      !JS_DefineProperty(s.cx, s.page, "IntersectionObserverEntry", entry_constructor, 0))
    return false;
  s.intersectionEntryPrototype = entry_prototype.get();
  JS::RootedObject prototype(s.cx), constructor(s.cx);
  if (!NewInterface(s, "IntersectionObserver", 1, IntersectionConstructor, &prototype,
                    &constructor)) return false;
  static const struct { const char* name; JSNative native; unsigned length; } methods[] = {
      {"observe", IntersectionObserve, 1}, {"unobserve", IntersectionUnobserve, 1},
      {"disconnect", IntersectionDisconnect, 0}, {"takeRecords", IntersectionTakeRecords, 0}};
  for (const auto& method : methods)
    if (!Method(s, "IntersectionObserver", prototype, method.name, method.native,
                method.length)) return false;
  static const char* const members[] = {"root", "rootMargin", "scrollMargin", "thresholds"};
  for (int32_t slot = 0; slot < 4; ++slot)
    if (!Getter(s, "IntersectionObserver", prototype, members[slot], IntersectionMember, slot))
      return false;
  if (!LinkInterface(s, prototype, constructor) ||
      !JS_DefineProperty(s.cx, s.page, "IntersectionObserver", constructor, 0)) return false;
  s.intersectionPrototype = prototype.get();
  JS::RootedObject deliver(s.cx, Function(s, IntersectionDeliver, "deliverIntersections", 0, 0));
  if (!deliver) return false;
  s.intersectionDeliver = deliver.get();
  return true;
}
}  // namespace ruyijs::web_observers
