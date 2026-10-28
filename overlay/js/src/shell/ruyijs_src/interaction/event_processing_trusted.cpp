#include "interaction/internal.h"
#include "runtime/page_helpers.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::interaction {
bool GetTrustedAcrossRealms(JSContext*, unsigned, JS::Value*);
namespace {
bool FontTrusted(JSContext* cx, unsigned n, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(n, vp);
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  State* owner = object ? Owner(cx, object, true) : nullptr;
  Event* event = owner ? owner->event(object) : nullptr;
  args.rval().setUndefined();
  if (event && event->kind == Kind::FontLoad) {
    ReportEventAccess(cx, event, "FontFaceSetLoadEvent", "isTrusted", int(ApiAccess::Read));
    args.rval().setBoolean(false);
  }
  return true;
}
}
bool InstallProcessingTrusted(State& state, JS::HandleObject object, Kind kind) {
  const bool audio = kind == Kind::AudioProcessing, font = kind == Kind::FontLoad;
  auto& getter = audio ? state.audioProcessingTrustedGetter :
                 font ? state.fontLoadTrustedGetter : state.taskPriorityTrustedGetter;
  const char* name = audio ? "AudioProcessingEvent" : font ? "FontFaceSetLoadEvent" : "TaskPriorityChangeEvent";
  if (!getter) {
    JSFunction* fn = JS_NewFunction(state.cx, font ? FontTrusted : GetTrustedAcrossRealms, 0, 0, "get isTrusted");
    if (!fn) return false;
    getter = JS_GetFunctionObject(fn);
    if (!state.helpers->Member(getter, name, "get isTrusted", "get")) return false;
  }
  JS::RootedObject setter(state.cx);
  return JS_DefineProperty(state.cx, object, "isTrusted", getter, setter,
    JSPROP_ENUMERATE|(kind==Kind::TaskPriority?JSPROP_PERMANENT:0));
}
}  // namespace ruyijs::interaction
