// CSSCounterStyleRule and CSSFontPaletteValuesRule: CSSRule subclasses Firefox
// 158 exposes (shapes measured, artifacts/mouser-datadome/iface_shape_probe.js).
// The parser does not produce @counter-style / @font-palette-values rules yet,
// so these interfaces have no instances: every member answers the receiver
// check with Firefox's text (DEVIATIONS D134).
#include "web_styling/cssom.h"
#include "js/Object.h"
#include "js/Wrapper.h"
#include <string>

namespace ruyijs::web_styling {
const JSClass kCounterStyleRuleClass = {"CSSCounterStyleRule", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kFontPaletteValuesRuleClass = {"CSSFontPaletteValuesRule", JSCLASS_HAS_RESERVED_SLOTS(1)};
namespace {
constexpr const char* kCounter[] = {"name", "system", "negative", "prefix", "suffix", "range", "pad",
                                    "fallback", "symbols", "additiveSymbols", "speakAs"};
constexpr const char* kPalette[] = {"name", "fontFamily", "basePalette", "overrideColors"};
// Slot: member index, +100 for the palette rule.
bool Report(JSContext* cx, const JS::CallArgs& args, const char* verb) {
  const int slot = WebKit::Slot(args);
  const bool palette = slot >= 100;
  const std::string message = std::string("'") + verb + " " + (palette ? kPalette[slot - 100] : kCounter[slot]) +
      "' called on an object that does not implement interface " +
      (palette ? "CSSFontPaletteValuesRule" : "CSSCounterStyleRule") + ".";
  return WebKit::Fail(cx, message.c_str());
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) { return Report(cx, JS::CallArgsFromVp(argc, vp), "get"); }
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) { return Report(cx, JS::CallArgsFromVp(argc, vp), "set"); }
bool Define(State& state, const char* name, const JSClass* clasp, WebFace& face) {
  auto& kit = state.kit;
  return kit.Define(name, clasp, state.rule.prototype, WebKit::Reject, &face) &&
         JS_SetPrototype(kit.cx, face.constructor, state.rule.constructor);
}
}
bool InstallAtRuleInterfaces(State& state) {
  auto& kit = state.kit;
  if (!Define(state, "CSSCounterStyleRule", &kCounterStyleRuleClass, state.counterStyleRule) ||
      !Define(state, "CSSFontPaletteValuesRule", &kFontPaletteValuesRuleClass, state.fontPaletteValuesRule))
    return false;
  for (int i = 0; i < 11; ++i)
    if (!kit.Accessor(state.counterStyleRule, "CSSCounterStyleRule", kCounter[i], Get, Set, i)) return false;
  for (int i = 0; i < 4; ++i)
    if (!kit.Accessor(state.fontPaletteValuesRule, "CSSFontPaletteValuesRule", kPalette[i], Get, nullptr, 100 + i))
      return false;
  return OrderConstructor(kit, state.counterStyleRule) && OrderConstructor(kit, state.fontPaletteValuesRule);
}
}
