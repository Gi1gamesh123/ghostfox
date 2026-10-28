// Interface objects and prototypes for CSS rule interfaces Firefox exposes but
// the parser never instantiates: members only answer the brand failure with
// Firefox's text. The member name is the accessor's own "get "/"set " name, so
// one shared native serves every accessor and every interface (indexed by slot).
#include "web_styling/rule_interface_members.h"
#include "js/Wrapper.h"
#include <cstddef>

namespace ruyijs::web_styling {
const JSClass kRuleInterfaceClass = {"CSSRuleInterface", JSCLASS_HAS_RESERVED_SLOTS(1)};

namespace {
constexpr const char* kInterfaceNames[SlotCount] = {
    "CSSContainerRule", "CSSFontFaceRule", "CSSFontFeatureValuesRule", "CSSImportRule",
    "CSSLayerBlockRule", "CSSLayerStatementRule", "CSSNamespaceRule", "CSSPositionTryRule",
    "CSSPropertyRule", "CSSScopeRule", "CSSStartingStyleRule", "CSSFontFaceDescriptors",
    "CSSPositionTryDescriptors"};
}
bool RuleInterfaceGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return WebKit::FailBrand(cx, args, kInterfaceNames[WebKit::Slot(args)]);
}
bool RuleInterfaceSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return WebKit::FailBrand(cx, args, kInterfaceNames[WebKit::Slot(args)]);
}
bool DefineRuleInterface(State& state, RuleInterfaceSlot slot, JS::HandleObject parentProto,
                         JS::HandleObject parentCtor, WebFace& face,
                         const RuleInterfaceMember* members, size_t count) {
  auto& kit = state.kit;
  const char* iface = kInterfaceNames[slot];
  if (!kit.Define(iface, &kRuleInterfaceClass, parentProto, WebKit::Reject, &face) ||
      !JS_SetPrototype(kit.cx, face.constructor, parentCtor)) return false;
  for (size_t i = 0; i < count; ++i)
    if (!kit.Accessor(face, iface, members[i].name, RuleInterfaceGet,
                      members[i].writable ? RuleInterfaceSet : nullptr, int32_t(slot)))
      return false;
  return OrderConstructor(kit, face);
}
}
