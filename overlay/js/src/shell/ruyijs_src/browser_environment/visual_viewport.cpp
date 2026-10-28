// VisualViewport dimensions share the Window content viewport source.
// pageLeft/pageTop follow this Context's simulated scroll
// offsets, offsetLeft/offsetTop are 0 because nothing pans the visual viewport,
// and scale is 1. The three EventHandler attributes are stored and never
// dispatched, like every other event handler this owner installs.
#include "browser_environment/visual_viewport.h"
#include "browser_environment/schema.h"
#include "browser_environment/state.h"
#include "interaction/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/Wrapper.h"
#include <iterator>

namespace ruyijs::browser_environment {
namespace {
const JSClass kViewportClass = {"VisualViewport", JSCLASS_HAS_RESERVED_SLOTS(4)};
enum Member : int32_t {
  kOffsetLeft = 600, kOffsetTop, kPageLeft, kPageTop, kWidth, kHeight, kScale,
  kOnResize = 620, kOnScroll, kOnScrollEnd
};
bool IllegalConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return Error(cx, "Illegal constructor.");
}
State* Owner(JSContext* cx, const JS::CallArgs& args) {
  JS::RootedObject receiver(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (receiver && JS::GetClass(receiver) == &kViewportClass) {
    auto slot = JS::GetReservedSlot(receiver, 0);
    if (!slot.isUndefined()) return static_cast<State*>(slot.toPrivate());
  }
  Error(cx, "VisualViewport member called on incompatible receiver");
  return nullptr;
}
const char* MemberName(int32_t slot) {
  switch (slot) {
    case kOffsetLeft: return "offsetLeft";
    case kOffsetTop: return "offsetTop";
    case kPageLeft: return "pageLeft";
    case kPageTop: return "pageTop";
    case kWidth: return "width";
    case kHeight: return "height";
    case kScale: return "scale";
    case kOnResize: return "onresize";
    case kOnScroll: return "onscroll";
    default: return "onscrollend";
  }
}
bool Getter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  State* state = Owner(cx, args);
  if (!state) return false;
  state->Observe("VisualViewport", MemberName(slot), int(ApiAccess::Read));
  double value = 0.0;
  switch (slot) {
    case kPageLeft: value = state->scrollX; break;
    case kPageTop: value = state->scrollY; break;
    case kWidth:
    case kHeight: {
      double width = 0, height = 0;
      if (!state->ReadViewport(&width, &height)) return false;
      value = slot == kWidth ? width : height;
      break;
    }
    case kScale: value = 1.0; break;
    case kOnResize:
    case kOnScroll:
    case kOnScrollEnd:
      args.rval().set(JS::GetReservedSlot(state->visualViewport,
                                          size_t(slot - kOnResize) + 1));
      return JS_WrapValue(cx, args.rval());
    default: value = 0.0; break;
  }
  args.rval().setDouble(value);
  return true;
}
bool Setter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  State* state = Owner(cx, args);
  if (!state) return false;
  if (slot < kOnResize || slot > kOnScrollEnd)
    return Error(cx, "VisualViewport member is read-only");
  state->Observe("VisualViewport", MemberName(slot), int(ApiAccess::Write));
  // WebIDL EventHandler: a non-callable value is stored as null.
  JS::RootedValue handler(cx);
  if (args.get(0).isObject() && JS::IsCallable(&args.get(0).toObject()))
    handler.set(args.get(0));
  else handler.setNull();
  {
    JSAutoRealm realm(cx, state->page);
    if (!JS_WrapValue(cx, &handler)) return false;
    JS::SetReservedSlot(state->visualViewport, size_t(slot - kOnResize) + 1, handler);
  }
  args.rval().setUndefined();
  return true;
}
bool Accessor(State& state, JS::HandleObject target, const char* name, int32_t slot,
              JSNative setter) {
  return state.Accessor(target, name, slot, JSPROP_ENUMERATE, Getter, setter) &&
         state.helpers->Accessor("VisualViewport", target, name);
}
}  // namespace

bool InstallVisualViewport(State& state, interaction::Realm& events) {
  JSContext* cx = state.cx;
  JS::RootedObject eventPrototype(cx, events.prototype());
  JS::RootedObject prototype(cx,
      JS_NewObjectWithGivenProto(cx, &kViewportClass, eventPrototype));
  JS::RootedObject constructor(cx,
      state.Function(IllegalConstructor, "VisualViewport", 0, 0, JSFUN_CONSTRUCTOR));
  JS::RootedObject instance(cx,
      JS_NewObjectWithGivenProto(cx, &kViewportClass, prototype));
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, kViewportClass.name));
  JS::RootedId tagId(cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  if (!eventPrototype || !prototype || !constructor || !instance || !tag ||
      !JS_DefineProperty(cx, constructor, "prototype", prototype,
                         JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(cx, prototype, "constructor", constructor, 0) ||
      !JS_DefinePropertyById(cx, prototype, tagId, tag, JSPROP_READONLY)) return false;
  static const char* const kReadonly[] = {"offsetLeft", "offsetTop", "pageLeft",
                                          "pageTop", "width", "height", "scale"};
  for (size_t index = 0; index < std::size(kReadonly); ++index) {
    if (!Accessor(state, prototype, kReadonly[index], int32_t(kOffsetLeft + index),
                  nullptr)) return false;
  }
  static const char* const kHandlers[] = {"onresize", "onscroll", "onscrollend"};
  for (size_t index = 0; index < std::size(kHandlers); ++index) {
    if (!Accessor(state, prototype, kHandlers[index], int32_t(kOnResize + index),
                  Setter)) return false;
  }
  for (size_t slot = 0; slot < 4; ++slot)
    JS::SetReservedSlot(instance, slot, slot == 0 ? JS::PrivateValue(&state)
                                                  : JS::NullValue());
  if (!events.Attach(instance)) return false;
  state.visualViewport = instance;
  return JS_DefineProperty(cx, state.page, "VisualViewport", constructor, 0) &&
         state.helpers->Interface("VisualViewport", prototype) &&
         state.helpers->Object("VisualViewport", prototype) &&
         state.helpers->Constructor("VisualViewport", constructor);
}
}  // namespace ruyijs::browser_environment
