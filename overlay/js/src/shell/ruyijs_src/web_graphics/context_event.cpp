// Event core/lifecycle remains in Interaction, created by its installed factory.
#include "web_graphics/internal.h"
#include "js/CallAndConstruct.h"
#include "js/WeakMap.h"
#include "runtime/native_constructor_trace.h"
namespace ruyijs::web_graphics {
namespace {
const JSClass kEventFace = {"WebGLContextEvent", 0};
bool StatusMessage(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  JS::RootedValue message(cx);
  if (!args.thisv().isObject() || !JS::GetWeakMapEntry(cx, s.eventMessages, args.thisv(), &message))
    return WebKit::Fail(cx, "Illegal invocation");
  if (!message.isString()) return WebKit::Fail(cx, "Illegal invocation");
  args.rval().set(message); return true;
}
bool EventCtor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  NativeConstructorTrace trace(cx, args);
  JS::RootedObject proto(cx);
  if (!s.kit.ConstructProto(args, s.event.prototype, &proto)) return false;
  if (!argc) return WebKit::Fail(cx, "WebGLContextEvent requires a type");
  JS::RootedString message(cx, JS_NewStringCopyZ(cx, ""));
  JS::RootedValue init(cx, args.get(1)), status(cx);
  if (init.isObject()) {
    JS::RootedObject options(cx, &init.toObject());
    if (!JS_GetProperty(cx, options, "statusMessage", &status)) return false;
    if (!status.isUndefined()) message = JS::ToString(cx, status);
  }
  if (!message) return false;
  JS::RootedValue constructor(cx, JS::ObjectValue(*s.eventFactory));
  JS::RootedValueArray<2> values(cx);
  values[0].set(args[0]); values[1].set(init);
  JS::RootedObject object(cx);
  if (!JS::Construct(cx, constructor, values, &object) || !JS_SetPrototype(cx, object, proto)) return false;
  JS::RootedValue key(cx, JS::ObjectValue(*object)), value(cx, JS::StringValue(message));
  if (!JS::SetWeakMapEntry(cx, s.eventMessages, key, value)) return false;
  args.rval().setObject(*object); return trace.Finish(true);
}
}
bool InstallContextEvent(State& s) {
  JS::RootedValue ctor(s.cx), proto(s.cx);
  if (!JS_GetProperty(s.cx, s.kit.page, "Event", &ctor) || !ctor.isObject()) return false;
  JS::RootedObject eventCtor(s.cx, &ctor.toObject());
  s.eventFactory = eventCtor;
  if (!JS_GetProperty(s.cx, eventCtor, "prototype", &proto) || !proto.isObject()) return false;
  JS::RootedObject parent(s.cx, &proto.toObject());
  s.eventMessages = JS::NewWeakMapObject(s.cx);
  return s.eventMessages && s.kit.Define("WebGLContextEvent", &kEventFace, parent, EventCtor, &s.event) &&
    s.kit.Accessor(s.event, "WebGLContextEvent", "statusMessage", StatusMessage, nullptr, 0) &&
    JS_DefineProperty(s.cx, s.event.constructor, "length", 1, JSPROP_READONLY);
}
}
