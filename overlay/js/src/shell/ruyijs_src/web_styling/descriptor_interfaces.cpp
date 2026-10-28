// CSSFontFaceDescriptors / CSSPositionTryDescriptors: CSSStyleDeclaration
// sub-interfaces with named descriptor accessors. The parser never produces
// @font-face / @position-try rules, so members only answer the brand failure;
// their prototypes also expose Firefox's value-iterator symbol.
#include "web_styling/rule_interface_members.h"
#include "js/Realm.h"
#include "js/Symbol.h"
#include <cstddef>

namespace ruyijs::web_styling {
namespace {
constexpr RuleInterfaceMember kFontFaceDescriptors[] = {
  {"fontFamily", true}, {"font-family", true}, {"src", true},
  {"fontStyle", true}, {"font-style", true}, {"fontWeight", true}, {"font-weight", true},
  {"fontWidth", true}, {"font-width", true}, {"font-stretch", true}, {"fontStretch", true},
  {"fontDisplay", true}, {"font-display", true}, {"unicodeRange", true}, {"unicode-range", true},
  {"fontFeatureSettings", true}, {"font-feature-settings", true},
  {"fontVariationSettings", true}, {"font-variation-settings", true},
  {"fontLanguageOverride", true}, {"font-language-override", true},
  {"ascentOverride", true}, {"ascent-override", true}, {"descentOverride", true}, {"descent-override", true},
  {"lineGapOverride", true}, {"line-gap-override", true}, {"sizeAdjust", true}, {"size-adjust", true}};
constexpr RuleInterfaceMember kPositionTryDescriptors[] = {
  {"positionArea", true}, {"position-area", true}, {"alignSelf", true}, {"align-self", true},
  {"justifySelf", true}, {"justify-self", true}, {"positionAnchor", true}, {"position-anchor", true},
  {"maxBlockSize", true}, {"max-block-size", true}, {"maxHeight", true}, {"max-height", true},
  {"maxInlineSize", true}, {"max-inline-size", true}, {"maxWidth", true}, {"max-width", true},
  {"bottom", true}, {"insetBlockEnd", true}, {"inset-block-end", true},
  {"insetBlockStart", true}, {"inset-block-start", true}, {"insetInlineEnd", true}, {"inset-inline-end", true},
  {"insetInlineStart", true}, {"inset-inline-start", true}, {"left", true}, {"right", true}, {"top", true},
  {"marginBlockEnd", true}, {"margin-block-end", true}, {"marginBlockStart", true}, {"margin-block-start", true},
  {"marginBottom", true}, {"margin-bottom", true}, {"marginInlineEnd", true}, {"margin-inline-end", true},
  {"marginInlineStart", true}, {"margin-inline-start", true}, {"marginLeft", true}, {"margin-left", true},
  {"marginRight", true}, {"margin-right", true}, {"marginTop", true}, {"margin-top", true},
  {"blockSize", true}, {"block-size", true}, {"height", true}, {"inlineSize", true}, {"inline-size", true},
  {"minBlockSize", true}, {"min-block-size", true}, {"minHeight", true}, {"min-height", true},
  {"minInlineSize", true}, {"min-inline-size", true}, {"minWidth", true}, {"min-width", true},
  {"width", true}, {"margin", true}, {"marginBlock", true}, {"margin-block", true},
  {"marginInline", true}, {"margin-inline", true}, {"paddingBlock", true}, {"padding-block", true},
  {"paddingInline", true}, {"padding-inline", true}, {"inset", true}, {"insetBlock", true}, {"inset-block", true},
  {"insetInline", true}, {"inset-inline", true}, {"placeSelf", true}, {"place-self", true},
  {"webkitAlignSelf", true}, {"WebkitAlignSelf", true}, {"-webkit-align-self", true},
  {"MozMarginStart", true}, {"-moz-margin-start", true}, {"MozMarginEnd", true}, {"-moz-margin-end", true}};

bool DescriptorSymbols(State& state, WebFace& face) {
  auto& kit = state.kit;
  JS::RootedId tag(kit.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(kit.cx, JS::SymbolCode::toStringTag)));
  JS::RootedValue tagValue(kit.cx);
  JS::ObjectOpResult removed;
  if (!JS_GetPropertyById(kit.cx, face.prototype, tag, &tagValue) ||
      !JS_DeletePropertyById(kit.cx, face.prototype, tag, removed) || !removed.ok()) return false;
  JS::RootedObject array(kit.cx, JS::GetRealmArrayPrototype(kit.cx));
  JS::RootedValue values(kit.cx);
  JS::RootedId iterator(kit.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(kit.cx, JS::SymbolCode::iterator)));
  return array && JS_GetProperty(kit.cx, array, "values", &values) &&
      JS_DefinePropertyById(kit.cx, face.prototype, iterator, values, 0) &&
      JS_DefinePropertyById(kit.cx, face.prototype, tag, tagValue, JSPROP_READONLY);
}
}
bool InstallCssDescriptorInterfaces(State& state) {
  auto& kit = state.kit;
  JS::RootedObject base(kit.cx);
  if (!JS_GetPrototype(kit.cx, state.declarationPrototype, &base) || !base) return false;
  JS::RootedValue constructor(kit.cx);
  if (!JS_GetProperty(kit.cx, base, "constructor", &constructor) || !constructor.isObject()) return false;
  JS::RootedObject parentCtor(kit.cx, &constructor.toObject());
  if (!DefineRuleInterface(state, SlotCSSFontFaceDescriptors, base, parentCtor,
                           state.fontFaceDescriptors, kFontFaceDescriptors,
                           std::size(kFontFaceDescriptors)) ||
      !DefineRuleInterface(state, SlotCSSPositionTryDescriptors, base, parentCtor,
                           state.positionTryDescriptors, kPositionTryDescriptors,
                           std::size(kPositionTryDescriptors)))
    return false;
  return DescriptorSymbols(state, state.fontFaceDescriptors) &&
      DescriptorSymbols(state, state.positionTryDescriptors);
}
}
