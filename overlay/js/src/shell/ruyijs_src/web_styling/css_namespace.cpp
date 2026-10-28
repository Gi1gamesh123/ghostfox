// The CSS namespace: CSS.escape and CSS.supports. Support is decided from the
// one styling owner's known-property table, so the CSSOM and the cascade agree
// on which properties this runtime understands.
#include "domain/styling/css_condition_value.h"
#include "domain/styling/css_identifier.h"
#include "js/Realm.h"
#include "runtime/page_helpers.h"
#include "web_styling/highlight.h"
#include "web_styling/internal.h"

namespace ruyijs::web_styling {
namespace {
enum CssMember { CssEscape = 0, CssSupports };

bool Escape(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if(!argc) return WebKit::Fail(cx,"CSS.escape requires an identifier");
  std::string input;
  if (!detail::ToAscii(cx, args.get(0), &input)) return false;
  std::string out = ruyijs::domain::styling::CssEscapeIdentifier(input);
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(out.data(), out.size())));
  if (!text) return false;
  args.rval().setString(text);
  return true;
}
bool Supports(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if(!argc) return WebKit::Fail(cx,"CSS.supports requires a condition");
  std::string property, value;
  if (args.length() >= 2) {
    if (!detail::ToAscii(cx, args.get(0), &property) ||
        !detail::ToAscii(cx, args.get(1), &value))
      return false;
  } else {
    std::string condition;
    if (!detail::ToAscii(cx, args.get(0), &condition)) return false;
    args.rval().setBoolean(ruyijs::domain::styling::CssSupportsCondition(condition));
    return true;
  }
  args.rval().setBoolean(ruyijs::domain::styling::CssSupportsProperty(property,value));
  return true;
}
}  // namespace

bool InstallCSS(State& state) {
  WebKit& kit = state.kit;
  JS::RootedObject page(kit.cx, state.page);
  JS::RootedObject css(kit.cx, JS_NewPlainObject(kit.cx));
  JS::RootedObject escape(kit.cx, kit.Function(Escape, "escape", 1, CssEscape));
  JS::RootedObject supports(kit.cx, kit.Function(Supports, "supports", 1, CssSupports));
  if (!css || !escape || !supports ||
      !JS_DefineProperty(kit.cx, css, "escape", escape, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(kit.cx, css, "supports", supports, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(kit.cx, page, "CSS", css, 0))
    return false;
  return kit.helpers->Interface("CSS", css) && kit.helpers->Object("CSS", css) &&
         InstallHighlight(state) && InstallHighlightRegistry(state, css);
}
}  // namespace ruyijs::web_styling
