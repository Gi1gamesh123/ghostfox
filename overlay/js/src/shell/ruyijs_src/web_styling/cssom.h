// Native CSSOM factories over the Styling owner's single source/cache.
#pragma once
#include "web_styling/internal.h"
#include "js/Proxy.h"

namespace ruyijs::web_styling {
enum SheetSlot { SheetData, SheetOwner, SheetWrappers, SheetList, SheetMedia,
                 SheetPendingText, SheetPendingPromise, SheetOwnerNode, SheetConstructorDocument,
                 SheetPendingLoad };
enum RuleSlot { RuleData, RuleOwner, RuleParent, RuleStyle, RuleAncestor,
                RuleWrappers, RuleList, RuleMedia };
enum ListSlot { ListOwner, ListSheet, ListDocument };
extern const JSClass kRuleListClass;
extern const JSClass kMediaListClass;
enum CssListKind { RuleCollection, MediaCollection };

// Indexed keyframes use the existing CSSOM proxy. Its private native object
// holds the same rule slots (and expando properties), not a second rule value.
inline JSObject* RuleStorage(JSObject* rule) {
  return JS::GetClass(rule)==&kKeyframesRuleClass?&js::GetProxyPrivate(rule).toObject():rule;
}
inline const JS::Value& RuleSlotValue(JSObject* rule,uint32_t slot) {
  return JS::GetReservedSlot(RuleStorage(rule),slot);
}
inline void SetRuleSlotValue(JSObject* rule,uint32_t slot,const JS::Value& value) {
  JS::SetReservedSlot(RuleStorage(rule),slot,value);
}

inline SheetCache& SheetDataOf(JS::HandleObject sheet) {
  return **static_cast<std::shared_ptr<SheetCache>*>(
      JS::GetReservedSlot(sheet, SheetData).toPrivate());
}
inline State& SheetOwnerOf(JS::HandleObject sheet) {
  return *static_cast<State*>(JS::GetReservedSlot(sheet, SheetOwner).toPrivate());
}
inline SheetRule& RuleDataOf(JS::HandleObject rule) {
  return **static_cast<std::shared_ptr<SheetRule>*>(
      RuleSlotValue(rule, RuleData).toPrivate());
}
inline State& RuleOwnerOf(JS::HandleObject rule) {
  return *static_cast<State*>(RuleSlotValue(rule, RuleOwner).toPrivate());
}
inline State& ListOwnerOf(JS::HandleObject list) {
  return *static_cast<State*>(js::GetProxyReservedSlot(list, ListOwner).toPrivate());
}
bool CssReceiver(JSContext*, JS::HandleValue, const JSClass*, JS::MutableHandleObject);
bool RuleReceiver(JSContext*, JS::HandleValue, JS::MutableHandleObject, bool styleOnly = false,
                    bool conditionOnly = false);
bool IsRuleObject(JS::HandleObject);
State& ContainerOwnerOf(JS::HandleObject);
std::vector<std::shared_ptr<SheetRule>>& ContainerRows(JS::HandleObject);
ruyijs::domain::styling::MediaListValue& ContainerMedia(JS::HandleObject);
bool InstallGroupingRules(State&);
bool InstallKeyframeRules(State&);
// CSSCounterStyleRule / CSSFontPaletteValuesRule interfaces (at_rule_interfaces.cpp).
bool InstallAtRuleInterfaces(State&);
bool RuleStyleGet(JSContext*, unsigned, JS::Value*);
bool RuleStyleSet(JSContext*, unsigned, JS::Value*);
bool CheckRuleDeclaration(State&, JS::HandleObject);
bool RuleGroupGet(JSContext*, unsigned, JS::Value*);
bool RuleGroupInsert(JSContext*, unsigned, JS::Value*);
bool RuleGroupDelete(JSContext*, unsigned, JS::Value*);
bool RuleConditionGet(JSContext*, unsigned, JS::Value*);
bool RuleMediaGet(JSContext*, unsigned, JS::Value*);
bool RuleMediaSet(JSContext*, unsigned, JS::Value*);
bool DetachRule(JS::HandleObject, bool clearAncestor = true);
bool ReadSheetSources(State&, std::vector<document_model::StyleSheetSource>*);
bool ReadDocumentSheetSources(State&, JS::HandleObject, std::vector<document_model::StyleSheetSource>*);
bool SameSheetSource(const SheetCache&, const document_model::StyleSheetSource&);
bool PrepareSheetSources(State&, std::vector<document_model::StyleSheetSource>*,
                           const document_model::StyleSheetScope* = nullptr, bool readyOnly = true,
                           bool retireMembership = true);
bool EnsureExternalSheet(State&, const std::shared_ptr<SheetCache>&);
std::string ExternalCorsMode(const SheetCache&);
void CancelExternalSheet(JS::HandleObject);
bool RetireSheet(State&, const std::shared_ptr<SheetCache>&);
bool StyleElementAction(State&, uint64_t, uint64_t, document_model::StyleElementAction,
                         bool, JS::MutableHandleValue);
bool RefreshSheetSource(JSContext*, JS::HandleObject);
bool SheetAt(State&, uint32_t, JS::MutableHandleObject, JS::HandleObject document = nullptr);
JSObject* SheetObject(State&, const std::shared_ptr<SheetCache>&, JS::MutableHandleObject,
                      JS::HandleObject prototype = nullptr);
void ParseSheet(SheetCache&, const document_model::StyleSheetSource&);
bool ShiftRuleWrappers(JSContext*, JS::HandleObject, uint32_t, bool inserting);
bool NewCssList(State&, JS::HandleObject sheet, JS::MutableHandleObject, CssListKind = RuleCollection,
                 JS::HandleObject document = nullptr);
bool CssListLength(JSContext*, JS::HandleObject, uint32_t*);
bool CssListAt(JSContext*, JS::HandleObject, uint32_t, JS::MutableHandleValue);
const js::BaseProxyHandler* CssListHandler();
bool InstallSheetList(State&);
bool InstallBaseSheet(State&);
bool InstallMediaList(State&);
bool CssString(JSContext*, JS::HandleValue, std::string*);
bool SheetConstruct(JSContext*, unsigned, JS::Value*);
bool SheetInsert(JSContext*, unsigned, JS::Value*);
bool SheetDelete(JSContext*, unsigned, JS::Value*);
bool SheetRules(JSContext*, unsigned, JS::Value*);
bool SheetLegacy(JSContext*, unsigned, JS::Value*);
bool SheetReplace(JSContext*, unsigned, JS::Value*);
bool InsertCssRule(JSContext*, JS::HandleObject, const std::string&, uint32_t);
bool DeleteCssRule(JSContext*, JS::HandleObject, uint32_t);
bool ReplaceRuleRows(JSContext*, JS::HandleObject, const std::string&);
bool OrderConstructor(WebKit&, WebFace&);
}  // namespace ruyijs::web_styling
