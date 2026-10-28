// ARIA nullable strings reflect the existing element's null-namespace attributes.
#include "document_model/state.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
struct Property { const char* name; const char16_t* attribute; };
constexpr Property kProperties[] = {
    {"role", u"role"},
    {"ariaAtomic", u"aria-atomic"},
    {"ariaAutoComplete", u"aria-autocomplete"},
    {"ariaBrailleLabel", u"aria-braillelabel"},
    {"ariaBrailleRoleDescription", u"aria-brailleroledescription"},
    {"ariaBusy", u"aria-busy"},
    {"ariaChecked", u"aria-checked"},
    {"ariaColCount", u"aria-colcount"},
    {"ariaColIndex", u"aria-colindex"},
    {"ariaColIndexText", u"aria-colindextext"},
    {"ariaColSpan", u"aria-colspan"},
    {"ariaCurrent", u"aria-current"},
    {"ariaDescription", u"aria-description"},
    {"ariaDisabled", u"aria-disabled"},
    {"ariaExpanded", u"aria-expanded"},
    {"ariaHasPopup", u"aria-haspopup"},
    {"ariaHidden", u"aria-hidden"},
    {"ariaInvalid", u"aria-invalid"},
    {"ariaKeyShortcuts", u"aria-keyshortcuts"},
    {"ariaLabel", u"aria-label"},
    {"ariaLevel", u"aria-level"},
    {"ariaLive", u"aria-live"},
    {"ariaModal", u"aria-modal"},
    {"ariaMultiLine", u"aria-multiline"},
    {"ariaMultiSelectable", u"aria-multiselectable"},
    {"ariaOrientation", u"aria-orientation"},
    {"ariaPlaceholder", u"aria-placeholder"},
    {"ariaPosInSet", u"aria-posinset"},
    {"ariaPressed", u"aria-pressed"},
    {"ariaReadOnly", u"aria-readonly"},
    {"ariaRelevant", u"aria-relevant"},
    {"ariaRequired", u"aria-required"},
    {"ariaRoleDescription", u"aria-roledescription"},
    {"ariaRowCount", u"aria-rowcount"},
    {"ariaRowIndex", u"aria-rowindex"},
    {"ariaRowIndexText", u"aria-rowindextext"},
    {"ariaRowSpan", u"aria-rowspan"},
    {"ariaSelected", u"aria-selected"},
    {"ariaSetSize", u"aria-setsize"},
    {"ariaSort", u"aria-sort"},
    {"ariaValueMax", u"aria-valuemax"},
    {"ariaValueMin", u"aria-valuemin"},
    {"ariaValueNow", u"aria-valuenow"},
    {"ariaValueText", u"aria-valuetext"}};

bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Element);
  if (!entry) return false;
  State& owner = *entry->owner;
  const auto& property = kProperties[FunctionSlot(args)];
  std::optional<DomString> value;
  {
    JSAutoRealm realm(cx, owner.page);
    owner.Observe("Element", property.name, int(ApiAccess::Read));
    if (!owner.Check(owner.model.GetAttributeNS(entry->key, {}, property.attribute, &value)))
      return false;
  }
  return Value(cx, value, args.rval());
}

bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!NodeReceiver(cx, args.thisv(), NodeKind::Element)) return false;
  const auto& property = kProperties[FunctionSlot(args)];
  const bool remove = args.get(0).isNullOrUndefined();
  DomString value;
  if (!remove && !String(cx, args.get(0), &value)) return false;
  // Conversion can adopt the receiver into another document or realm.
  Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Element);
  if (!entry) return false;
  State& owner = *entry->owner;
  {
    JSAutoRealm realm(cx, owner.page);
    owner.Observe("Element", property.name, int(ApiAccess::Write));
    const Status status = remove
        ? owner.model.RemoveAttributeNS(entry->key, {}, property.attribute)
        : owner.model.SetAttributeNS(entry->key, {}, property.attribute, value);
    if (!owner.Check(status)) return false;
  }
  args.rval().setUndefined();
  return true;
}
}

bool InstallElementARIA(State& state) {
  for (size_t index = 0; index < std::size(kProperties); ++index)
    if (!state.Accessor("Element", kProperties[index].name, Get, int32_t(index), Set))
      return false;
  return true;
}
}
