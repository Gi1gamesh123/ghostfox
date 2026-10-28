#include "document_model/html_select.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"

namespace ruyijs::document_model {
bool SelectWillValidate(Wrapper& entry, bool* out) {
  std::optional<DomString> disabled;
  if (!entry.owner->Check(entry.owner->model.GetAttribute(entry.key, u"disabled", &disabled))) return false;
  *out = !disabled.has_value(); return true;
}
namespace {
bool Option(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing())
    return TypeError(cx, "Failed to construct 'Option': Constructor Option requires 'new'");
  JS_ReportErrorASCII(cx, "Failed to construct 'Option': Option() constructor not yet implemented - requires document access");
  return false;
}
}
bool InstallSelectElements(State& state) {
  if (!InstallSelectAttributes(state) || !InstallSelectMethods(state) ||
      !InstallOptionsCollection(state) || !InstallControlValidity(state)) return false;
  JS::RootedObject factory(state.cx, state.Function(Option, "Option", 4, 0, JSFUN_CONSTRUCTOR));
  JS::RootedObject prototype(state.cx, state.Find("HTMLOptionElement")->prototype);
  return factory && JS_DefineProperty(state.cx, factory, "prototype", prototype,
      JSPROP_READONLY | JSPROP_PERMANENT) &&
      JS_DefineProperty(state.cx, state.page, "Option", factory, 0) &&
      state.helpers.Constructor("Option", factory);
}
}
