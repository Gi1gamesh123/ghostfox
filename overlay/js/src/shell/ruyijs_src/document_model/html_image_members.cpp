#include "document_model/html_control.h"
#include "js/Conversions.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
bool ImageDecode(JSContext*, unsigned, JS::Value*);
namespace {
enum Kind { Text, Cors, Border, Boolean, Spacing, Zero };
struct Member { const char* name; const char16_t* attr; Kind kind; };
constexpr Member kMembers[] = {
  {"alt", u"alt", Text}, {"srcset", u"srcset", Text}, {"sizes", u"sizes", Text},
  {"crossOrigin", u"crossorigin", Cors}, {"useMap", u"usemap", Text}, {"isMap", u"ismap", Boolean},
  {"referrerPolicy", u"referrerpolicy", Text}, {"decoding", u"decoding", Text},
  {"fetchPriority", u"fetchpriority", Text}, {"loading", u"loading", Text},
  {"name", u"name", Text}, {"lowsrc", u"lowsrc", Text}, {"align", u"align", Text},
  {"hspace", u"hspace", Spacing}, {"vspace", u"vspace", Spacing},
  {"longDesc", u"longdesc", Text}, {"border", u"border", Border},
  {"x", u"x", Zero}, {"y", u"y", Zero}
};
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& member = kMembers[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLImageElement");
  if (!entry) return false;
  auto& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLImageElement", member.name, int(ApiAccess::Read));
    if (member.kind == Zero) result.setInt32(0);
    else if (member.kind == Spacing) {
      uint32_t number;
      if (!state.Check(state.model.ImageSpacing(entry->key, member.attr, &number))) return false;
      result.setNumber(number);
    } else {
      std::optional<DomString> text;
      if (member.kind == Boolean) {
        if (!state.Check(state.model.GetAttribute(entry->key, member.attr, &text))) return false;
        result.setBoolean(text.has_value());
      } else {
        if (!state.Check(state.model.ImageAttribute(entry->key, member.attr, &text))) return false;
        if (!text) result.setNull();
        else if (!Value(cx, DomStringView(*text), &result)) return false;
      }
    }
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Write(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& member = kMembers[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLImageElement");
  if (!entry) return false;
  const bool remove = (member.kind == Cors && args.get(0).isNullOrUndefined()) ||
      (member.kind == Boolean && !JS::ToBoolean(args.get(0)));
  DomString text; uint32_t number = 0;
  if (member.kind == Spacing) {
    if (!JS::ToUint32(cx, args.get(0), &number)) return false;
  } else if (!remove && member.kind != Boolean &&
             !(member.kind == Border && args.get(0).isNull()) &&
             !ControlString(cx, args.get(0), &text)) return false;
  // Conversion may re-enter DOM adoption; consult the wrapper's current owner.
  auto& state = *entry->owner;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLImageElement", member.name, int(ApiAccess::Write));
    auto status = remove ? state.model.RemoveAttribute(entry->key, member.attr)
        : member.kind == Spacing ? state.model.SetImageSpacing(entry->key, member.attr, number)
                                 : state.model.SetAttribute(entry->key, member.attr, text);
    if (!state.Check(status)) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallImageMembers(State& state) {
  for (size_t i = 0; i < std::size(kMembers); ++i)
    if (!state.Accessor("HTMLImageElement", kMembers[i].name, Read, int32_t(i),
                        kMembers[i].kind == Zero ? nullptr : Write)) return false;
  return state.Method("HTMLImageElement", "decode", ImageDecode, 0, 0);
}
}
