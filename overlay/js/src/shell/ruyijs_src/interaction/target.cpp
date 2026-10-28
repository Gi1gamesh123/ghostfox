#include "interaction/internal.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "js/GlobalObject.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::interaction {
bool TargetConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return Error(cx, "TypeError", "EventTarget requires new");
  State* state = activeState;
  while (state && state->page != JS::CurrentGlobalOrNull(cx)) state = state->previous;
  if (!state) return Error(cx, "TypeError", "EventTarget realm is not installed");
  JS::RootedObject object(cx, JS_NewObjectForConstructor(cx, &kTargetClass, args));
  if (!object || !state->attach(object, true, nullptr)) return false;
  args.rval().setObject(*object);
  return trace.Finish(true);
}
bool InstallTarget(State& state) {
  static const JSFunctionSpec methods[] = {
    JS_FN("addEventListener", Add, 2, JSPROP_ENUMERATE),
    JS_FN("removeEventListener", Remove, 2, JSPROP_ENUMERATE),
    JS_FN("dispatchEvent", Dispatch, 1, JSPROP_ENUMERATE), JS_FS_END
  };
  JS::RootedObject prototype(state.cx, JS_InitClass(state.cx, state.page, &kTargetClass,
      nullptr, "EventTarget", TargetConstructor, 0, nullptr, methods, nullptr, nullptr));
  if (!prototype || !Tag(state.cx, prototype, "EventTarget")) return false;
  state.targetPrototype = prototype;
  JS::RootedValue value(state.cx);
  if (!JS_GetProperty(state.cx, state.page, "EventTarget", &value)) return false;
  JS::RootedObject constructor(state.cx, &value.toObject());
  if(!JS_DeleteProperty(state.cx,prototype,"constructor")||
     !JS_DefineProperty(state.cx,prototype,"constructor",value,0))return false;
  return state.helpers->Interface("EventTarget", prototype) &&
         state.helpers->Constructor("EventTarget", constructor);
}
}
