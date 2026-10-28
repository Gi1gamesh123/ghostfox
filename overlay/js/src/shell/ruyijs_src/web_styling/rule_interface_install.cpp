// CSSRule sub-interfaces Firefox 158 exposes on Window but the parser does not
// instantiate. Each interface sits at its WebIDL parent branch and shares the
// "no instances" brand-failure members (rule_interface_members.cpp).
#include "web_styling/rule_interface_members.h"
#include <cstddef>

namespace ruyijs::web_styling {
bool InstallCssDescriptorInterfaces(State& state);
namespace {
constexpr RuleInterfaceMember kContainer[] = {{"containerName", false}, {"containerQuery", false}, {"conditions", false}};
constexpr RuleInterfaceMember kFontFace[] = {{"style", true}};
constexpr RuleInterfaceMember kFontFeatureValues[] = {{"fontFamily", true}, {"valueText", true}};
constexpr RuleInterfaceMember kImport[] = {{"href", false}, {"media", true}, {"styleSheet", false},
                                           {"layerName", false}, {"supportsText", false}};
constexpr RuleInterfaceMember kLayerBlock[] = {{"name", false}};
constexpr RuleInterfaceMember kLayerStatement[] = {{"nameList", false}};
constexpr RuleInterfaceMember kNamespace[] = {{"namespaceURI", false}, {"prefix", false}};
constexpr RuleInterfaceMember kPositionTry[] = {{"name", false}, {"style", true}};
constexpr RuleInterfaceMember kProperty[] = {{"name", false}, {"syntax", false}, {"inherits", false}, {"initialValue", false}};
constexpr RuleInterfaceMember kScope[] = {{"start", false}, {"end", false}};
}
bool InstallCssRuleInterfaces(State& state) {
  return DefineRuleInterface(state, SlotCSSContainerRule, state.conditionRule.prototype,
                             state.conditionRule.constructor, state.containerRule,
                             kContainer, std::size(kContainer)) &&
      DefineRuleInterface(state, SlotCSSLayerBlockRule, state.groupingRule.prototype,
                          state.groupingRule.constructor, state.layerBlockRule,
                          kLayerBlock, std::size(kLayerBlock)) &&
      DefineRuleInterface(state, SlotCSSScopeRule, state.groupingRule.prototype,
                          state.groupingRule.constructor, state.scopeRule,
                          kScope, std::size(kScope)) &&
      DefineRuleInterface(state, SlotCSSStartingStyleRule, state.groupingRule.prototype,
                          state.groupingRule.constructor, state.startingStyleRule, nullptr, 0) &&
      DefineRuleInterface(state, SlotCSSFontFaceRule, state.rule.prototype, state.rule.constructor,
                          state.fontFaceRule, kFontFace, std::size(kFontFace)) &&
      DefineRuleInterface(state, SlotCSSFontFeatureValuesRule, state.rule.prototype,
                          state.rule.constructor, state.fontFeatureValuesRule,
                          kFontFeatureValues, std::size(kFontFeatureValues)) &&
      DefineRuleInterface(state, SlotCSSImportRule, state.rule.prototype, state.rule.constructor,
                          state.importRule, kImport, std::size(kImport)) &&
      DefineRuleInterface(state, SlotCSSLayerStatementRule, state.rule.prototype,
                          state.rule.constructor, state.layerStatementRule,
                          kLayerStatement, std::size(kLayerStatement)) &&
      DefineRuleInterface(state, SlotCSSNamespaceRule, state.rule.prototype, state.rule.constructor,
                          state.namespaceRule, kNamespace, std::size(kNamespace)) &&
      DefineRuleInterface(state, SlotCSSPositionTryRule, state.rule.prototype, state.rule.constructor,
                          state.positionTryRule, kPositionTry, std::size(kPositionTry)) &&
      DefineRuleInterface(state, SlotCSSPropertyRule, state.rule.prototype, state.rule.constructor,
                          state.propertyRule, kProperty, std::size(kProperty)) &&
      InstallCssDescriptorInterfaces(state);
}
}
