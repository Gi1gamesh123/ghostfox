// Document.styleSheets installation remains the original Styling owner path.
#include "web_styling/cssom.h"
#include "web_styling/adopted.h"
#include "web_styling/realm.h"
#include "js/Realm.h"

namespace ruyijs::web_styling {
namespace {
bool SheetsGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  if (!state.documentResolve) return WebKit::Fail(cx, "Document CSS receiver port is not installed");
  return state.documentResolve(state.documentContext, args.thisv(), args.rval(), WebKit::Slot(args) == 1);
}
}
bool Realm::NewDocumentList(JS::HandleObject document, JS::MutableHandleValue out) {
  State& state = *state_;
  JS::RootedObject source(state.cx, document),list(state.cx);
  JS::RootedValue result(state.cx);
  {
    JSAutoRealm realm(state.cx, state.page);
    if (!JS_WrapObject(state.cx, &source) ||
        !NewCssList(state, nullptr, &list, RuleCollection, source)) return false;
    result.setObject(*list);
  }
  if (!JS_WrapValue(state.cx, &result)) return false;
  out.set(result);return true;
}
bool InstallSheets(State& state) {
  return InstallBaseSheet(state) && InstallMediaList(state) &&
         InstallSheetClass(state) && InstallRules(state) &&
         InstallRuleList(state) && InstallSheetList(state) &&
         detail::AccessorOn(state.kit, state.documentPrototype, "Document", "styleSheets",
                            SheetsGet, nullptr, 0) &&
         detail::AccessorOn(state.kit, state.shadowPrototype, "ShadowRoot", "styleSheets",
                            SheetsGet, nullptr, 1) && InstallAdoptedStyles(state);
}
}  // namespace ruyijs::web_styling
