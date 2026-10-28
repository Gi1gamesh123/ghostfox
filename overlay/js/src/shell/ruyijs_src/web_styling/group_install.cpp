// One installer for grouping/condition/media/supports, at Firefox IDL paths.
#include "web_styling/cssom.h"
namespace ruyijs::web_styling {
bool InstallGroupingRules(State& state) {
  auto& kit=state.kit;
  if(!kit.Define("CSSGroupingRule",&kGroupingRuleClass,state.rule.prototype,WebKit::Reject,&state.groupingRule)||
      !kit.Define("CSSConditionRule",&kConditionRuleClass,state.groupingRule.prototype,WebKit::Reject,&state.conditionRule)||
      !kit.Define("CSSMediaRule",&kMediaRuleClass,state.conditionRule.prototype,WebKit::Reject,&state.mediaRule)||
      !kit.Define("CSSSupportsRule",&kSupportsRuleClass,state.conditionRule.prototype,WebKit::Reject,&state.supportsRule)) return false;
  if(!JS_SetPrototype(kit.cx,state.groupingRule.constructor,state.rule.constructor)||
      !JS_SetPrototype(kit.cx,state.conditionRule.constructor,state.groupingRule.constructor)||
      !JS_SetPrototype(kit.cx,state.mediaRule.constructor,state.conditionRule.constructor)||
      !JS_SetPrototype(kit.cx,state.supportsRule.constructor,state.conditionRule.constructor)) return false;
  return kit.Method(state.groupingRule,"CSSGroupingRule","insertRule",RuleGroupInsert,1,0)&&
      kit.Method(state.groupingRule,"CSSGroupingRule","deleteRule",RuleGroupDelete,1,0)&&
      kit.Accessor(state.groupingRule,"CSSGroupingRule","cssRules",RuleGroupGet,nullptr,0)&&
      OrderConstructor(kit,state.groupingRule)&&
      kit.Accessor(state.conditionRule,"CSSConditionRule","conditionText",RuleConditionGet,nullptr,0)&&
      OrderConstructor(kit,state.conditionRule)&&
      kit.Accessor(state.mediaRule,"CSSMediaRule","media",RuleMediaGet,RuleMediaSet,0)&&
      OrderConstructor(kit,state.mediaRule)&&OrderConstructor(kit,state.supportsRule);
}
}
