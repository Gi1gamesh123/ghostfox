// CanvasRenderingContext2D: one context object per canvas, created by the canvas
// member and owned by the realm that installed the canvas interfaces.
#include "web_canvas/internal.h"

#include "js/Realm.h"
#include "runtime/native_constructor_trace.h"

namespace ruyijs::web_canvas {
const JSClass kContext2DClass = {"CanvasRenderingContext2D",
                                 JSCLASS_HAS_RESERVED_SLOTS(kContextSlotCount)};

const JSClass kOffscreen2DClass = {"OffscreenCanvasRenderingContext2D",
    JSCLASS_HAS_RESERVED_SLOTS(kContextSlotCount)};

JSObject* NewContext2D(State& state, JS::HandleObject canvas, JS::MutableHandleObject out) {
  JSContext* cx = state.cx;
  JS::RootedObject proto(cx, JS::GetClass(canvas) == &kOffscreenClass
      ? state.offscreen2d.prototype.get() : state.context2d.prototype.get());
  JS::RootedObject object(cx, state.kit.New(JS::GetClass(canvas) == &kOffscreenClass ? &kOffscreen2DClass : &kContext2DClass, proto));
  JS::RootedObject drawing(cx);
  JS::RootedObject stack(cx, JS::NewArrayObject(cx, 0));
  JS::RootedObject path(cx, JS::NewArrayObject(cx, 0));
  if (!object || !stack || !path || !detail::NewDrawingState(cx, &drawing))
    return nullptr;
  JS::SetReservedSlot(object, kContextCanvasSlot, JS::ObjectValue(*canvas));
  JS::SetReservedSlot(object, kContextStateSlot, JS::ObjectValue(*drawing));
  JS::SetReservedSlot(object, kContextStackSlot, JS::ObjectValue(*stack));
  JS::SetReservedSlot(object, kContextPathSlot, JS::ObjectValue(*path));
  JS::SetReservedSlot(object, kContextAlphaSlot, JS::BooleanValue(true));
  JS::SetReservedSlot(object, kContextReadOftenSlot, JS::BooleanValue(false));
  out.set(object);
  return object;
}

bool Context2DCtor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  // CanvasRenderingContext2D has no constructor in the specification.
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}

bool Context2DCanvas(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, kContextCanvasSlot));
  return true;
}

bool InstallContext2D(State& state) {
  WebKit& kit = state.kit;
  JSContext* cx = state.cx;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  if (!kit.Define("CanvasRenderingContext2D", &kContext2DClass, objectProto, Context2DCtor,
                  &state.context2d)) return false;
  return InstallContextMembers(state, state.context2d) &&
         detail::Method2D(state,state.context2d,"getContextAttributes",ContextSettings,0,0) &&
         detail::Method2D(state,state.context2d,"drawFocusIfNeeded",DrawFocus,1,0);
}
bool InstallContextMembers(State& state, WebFace& face) {
  return detail::Accessor2D(state,face,"canvas",Context2DCanvas,nullptr,0) &&
         detail::Method2D(state,face,"save",Context2DSave,0,0) &&
         detail::Method2D(state,face,"restore",Context2DRestore,0,0) &&
         InstallContext2DState(state, face) && InstallContext2DPath(state, face) &&
         InstallContext2DFill(state, face) && InstallContext2DTransform(state, face) &&
         InstallImageMethods(state, face) && InstallContext2DExtra(state, face);
}
}  // namespace ruyijs::web_canvas
