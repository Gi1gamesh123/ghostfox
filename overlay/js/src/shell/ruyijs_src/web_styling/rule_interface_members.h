// Shared "interface only, no instances" helpers for the CSS rule interfaces.
// Every member answers Firefox's brand failure; the parser does not produce
// these rules, so no instance ever passes the receiver check (DEVIATIONS D140).
#ifndef RUYIJS_WEB_STYLING_RULE_INTERFACE_MEMBERS_H
#define RUYIJS_WEB_STYLING_RULE_INTERFACE_MEMBERS_H
#include "web_styling/cssom.h"
namespace ruyijs::web_styling {
struct RuleInterfaceMember {
  const char* name;
  bool writable;
};
// Slot on every accessor of an interface; RuleInterfaceGet/Set read it to pick
// the interface name, while the member name comes from the callee's own name.
enum RuleInterfaceSlot {
  SlotCSSContainerRule = 0,
  SlotCSSFontFaceRule,
  SlotCSSFontFeatureValuesRule,
  SlotCSSImportRule,
  SlotCSSLayerBlockRule,
  SlotCSSLayerStatementRule,
  SlotCSSNamespaceRule,
  SlotCSSPositionTryRule,
  SlotCSSPropertyRule,
  SlotCSSScopeRule,
  SlotCSSStartingStyleRule,
  SlotCSSFontFaceDescriptors,
  SlotCSSPositionTryDescriptors,
  SlotCount
};
bool RuleInterfaceGet(JSContext* cx, unsigned argc, JS::Value* vp);
bool RuleInterfaceSet(JSContext* cx, unsigned argc, JS::Value* vp);
bool DefineRuleInterface(State& state, RuleInterfaceSlot slot, JS::HandleObject parentProto,
                         JS::HandleObject parentCtor, WebFace& face,
                         const RuleInterfaceMember* members, size_t count);
bool InstallCssRuleInterfaces(State& state);
}
#endif
