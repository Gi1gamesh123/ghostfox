#include "web_styling/cssom.h"
namespace ruyijs::web_styling {
namespace {
bool OwnerRule(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CssReceiver(cx, args.thisv(), &kSheetClass, &self)) return false;
  args.rval().setNull();return true;
}
}
bool InstallSheetClass(State& state) {
  auto& kit = state.kit;auto& face = state.sheet;
  if (!kit.Define("CSSStyleSheet", &kSheetClass, state.baseSheet.prototype, SheetConstruct, &face))
    return false;
  JS::ObjectOpResult deleted;
  return JS_DeleteProperty(kit.cx, face.prototype, "constructor", deleted) &&
      kit.Method(face, "CSSStyleSheet", "insertRule", SheetInsert, 1, 0) &&
      kit.Method(face, "CSSStyleSheet", "deleteRule", SheetDelete, 1, 0) &&
      kit.Method(face, "CSSStyleSheet", "replace", SheetReplace, 1, 1) &&
      kit.Method(face, "CSSStyleSheet", "replaceSync", SheetReplace, 1, 0) &&
      kit.Method(face, "CSSStyleSheet", "removeRule", SheetLegacy, 0, 0) &&
      kit.Method(face, "CSSStyleSheet", "addRule", SheetLegacy, 0, 1) &&
      kit.Accessor(face, "CSSStyleSheet", "ownerRule", OwnerRule, nullptr, 0) &&
      kit.Accessor(face, "CSSStyleSheet", "cssRules", SheetRules, nullptr, 0) &&
      kit.Accessor(face, "CSSStyleSheet", "rules", SheetRules, nullptr, 0) &&
      OrderConstructor(kit, face);
}
}  // namespace ruyijs::web_styling
