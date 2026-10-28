// AnimationTimeline, DocumentTimeline and the Document members timeline and
// getAnimations (docs/owners/WEB_ANIMATION.md). A timeline is active only for
// its realm's main document; its time is the Scheduling owner's one clock.
#include "web_animation/state.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/WeakMap.h"
#include <cmath>

namespace ruyijs::web_animation {
const JSClass kTimelineClass = {"DocumentTimeline", JSCLASS_HAS_RESERVED_SLOTS(kTCount)};
namespace {
bool ClearTick(JSContext*, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  static_cast<State*>(WebKit::Owner(args))->tick.reset();
  args.rval().setUndefined();
  return true;
}
std::optional<double> Tick(State& s) {
  if (s.tick) return s.tick;
  JS::RootedObject page(s.cx, s.kit.page);
  const double now = s.scheduling.now(s.scheduling.context, page);
  if (!std::isfinite(now)) return std::nullopt;
  JS::RootedObject task(s.cx, NewTask(s, ClearTick, page));
  // Without a task port the sample is not cached, so time never freezes forever.
  if (!task || !s.scheduling.post || !s.scheduling.post(s.scheduling.context, task)) {
    JS_ClearPendingException(s.cx);
    return now;
  }
  s.tick = now;
  return now;
}
JSObject* NewTimeline(State& s, JS::HandleObject proto, JS::HandleObject document, double origin) {
  JSObject* timeline = s.kit.New(&kTimelineClass, proto);
  if (!timeline) return nullptr;
  JS::SetReservedSlot(timeline, kTOwner, JS::PrivateValue(&s));
  JS::SetReservedSlot(timeline, kTDocument, document ? JS::ObjectValue(*document) : JS::NullValue());
  JS::SetReservedSlot(timeline, kTOrigin, JS::DoubleValue(origin));
  return timeline;
}
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
// new DocumentTimeline(optional DocumentTimelineOptions options = {})
bool TimelineCreate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  if (!args.isConstructing()) return TypeFail(cx, "DocumentTimeline constructor: 'new' is required");
  double origin = 0;
  JS::HandleValue options = args.get(0);
  if (!options.isNullOrUndefined()) {
    if (!options.isObject())
      return TypeFail(cx, "DocumentTimeline constructor: Argument 1 can't be converted to a dictionary.");
    JS::RootedObject dictionary(cx, &options.toObject());
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, dictionary, "originTime", &value)) return false;
    if (!value.isUndefined()) {
      if (!JS::ToNumber(cx, value, &origin)) return false;
      if (!std::isfinite(origin))
        return TypeFail(cx, "DocumentTimeline constructor: 'originTime' member of "
                            "DocumentTimelineOptions is not a finite floating-point value.");
    }
  }
  JS::RootedObject proto(cx), document(cx, s.dom.mainDocument(s.dom.context));
  if (!s.kit.ConstructProto(args, s.documentTimeline.prototype, &proto)) return false;
  JSObject* timeline = NewTimeline(s, proto, document, origin);
  if (!timeline) return false;
  args.rval().setObject(*timeline);
  return trace.Finish(true);
}
bool CurrentTimeGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JSObject* self = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (!self || JS::GetClass(self) != &kTimelineClass)
    return NotImplemented(cx, "get currentTime", "AnimationTimeline");
  const auto time = TimelineTime(self);
  if (time) args.rval().setDouble(*time); else args.rval().setNull();
  return true;
}
bool DocumentMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const bool timeline = WebKit::Slot(args) == 0;
  if (s.dom.kind(s.dom.context, args.thisv()) != 2)
    return NotImplemented(cx, timeline ? "get timeline" : "getAnimations", "Document");
  JS::RootedObject document(cx, &args.thisv().toObject()), result(cx);
  if (!timeline) return AnimationsFor(s, document, true, args.rval());
  if (!DocumentTimeline(s, document, &result)) return false;
  args.rval().setObject(*result);
  return JS_WrapValue(cx, args.rval());
}
}  // namespace

std::optional<double> TimelineTime(JSObject* timeline) {
  if (!timeline || JS::GetClass(timeline) != &kTimelineClass) return std::nullopt;
  State& s = OwnerOf(timeline);
  const JS::Value document = JS::GetReservedSlot(timeline, kTDocument);
  if (!s.scheduling.now || !s.dom.mainDocument || document.isNull() ||
      &document.toObject() != s.dom.mainDocument(s.dom.context))
    return std::nullopt;
  const auto now = Tick(s);
  if (!now) return std::nullopt;
  return *now - JS::GetReservedSlot(timeline, kTOrigin).toDouble();
}

// The document's one DocumentTimeline (SameObject), created on first use.
bool DocumentTimeline(State& s, JS::HandleObject document, JS::MutableHandleObject result) {
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedObject key(cx, document);
  JS::RootedValue keyValue(cx, JS::ObjectValue(*key)), value(cx);
  if (!JS_WrapValue(cx, &keyValue)) return false;
  JS::RootedObject map(cx, s.timelines), wrapped(cx, &keyValue.toObject());
  if (!JS::GetWeakMapEntry(cx, map, keyValue, &value)) return false;
  if (value.isObject()) { result.set(&value.toObject()); return true; }
  JS::RootedObject proto(cx, s.documentTimeline.prototype);
  result.set(NewTimeline(s, proto, wrapped, 0));
  if (!result) return false;
  value.setObject(*result);
  return JS::SetWeakMapEntry(cx, map, keyValue, value);
}

bool InstallTimelines(State& s, JS::HandleObject documentPrototype) {
  JSContext* cx = s.cx;
  JS::RootedObject object(cx, JS::GetRealmObjectPrototype(cx));
  if (!s.kit.Define("AnimationTimeline", &kTimelineClass, object, Illegal, &s.timeline) ||
      !s.kit.Accessor(s.timeline, "AnimationTimeline", "currentTime", CurrentTimeGet, nullptr, 0) ||
      !s.kit.Define("DocumentTimeline", &kTimelineClass, s.timeline.prototype, TimelineCreate,
                    &s.documentTimeline))
    return false;
  WebFace document(cx);
  document.prototype = documentPrototype;
  return s.kit.Accessor(document, "Document", "timeline", DocumentMember, nullptr, 0) &&
         s.kit.Method(document, "Document", "getAnimations", DocumentMember, 0, 1);
}
}  // namespace ruyijs::web_animation
