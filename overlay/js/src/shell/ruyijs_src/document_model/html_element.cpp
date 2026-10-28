// HTMLElement's reflected members and the shared installer (T4). Owner:
// document_model. Reflection reads and writes the element's attributes through
// the unique Model; style/dataset and click live in html_element_actions.cpp.
// offset* read the element owner's layout port (Web Geometry); focus/blur stay
// inert because there is no focus owner (D20).
#include "document_model/state.h"
#include "document_model/html_element_internal.h"
#include "document_model/style_internal.h"
#include "domain/document_model/html_direction.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include <cmath>
#include <cstdint>

namespace ruyijs::document_model {
namespace {
enum Prop { Title, Dir, Lang, AccessKey, ContentEditable, IsContentEditable, Hidden,
            Draggable, Spellcheck, TabIndex, InnerText, OffsetWidth, OffsetHeight,
            OffsetTop, OffsetLeft, OffsetParent };

const char16_t* Attribute(Prop prop) {
  switch (prop) {
    case Title: return u"title";
    case Dir: return u"dir";
    case Lang: return u"lang";
    case AccessKey: return u"accesskey";
    case ContentEditable: return u"contenteditable";
    case IsContentEditable: return u"contenteditable";
    case TabIndex: return u"tabindex";
    case Hidden: return u"hidden";
    case Draggable: return u"draggable";
    case Spellcheck: return u"spellcheck";
    default: return u"";
  }
}
int32_t ParseInt(DomStringView text) {
  size_t i = 0;
  bool negative = false;
  while (i < text.size() && Blank(text[i])) ++i;
  if (i < text.size() && (text[i] == u'-' || text[i] == u'+')) {
    negative = text[i] == u'-';
    ++i;
  }
  int64_t value = 0;
  bool any = false;
  while (i < text.size() && text[i] >= u'0' && text[i] <= u'9') {
    value = value * 10 + (text[i] - u'0');
    any = true;
    if (value > INT64_C(2147483647)) break;
    ++i;
  }
  if (!any) return 0;
  if (negative) value = -value;
  if (value > INT64_C(2147483647)) value = INT64_C(2147483647);
  if (value < -INT64_C(2147483648)) value = -INT64_C(2147483648);
  return int32_t(value);
}
DomString Format(int32_t value) {
  std::string text = std::to_string(value);
  DomString out;
  for (char c : text) out.push_back(char16_t(c));
  return out;
}
// nsGenericHTMLElement::GetOffsetRect rounds each component to CSS pixels.
bool OffsetGet(JSContext* cx, const JS::CallArgs& args, Prop prop, Wrapper* entry) {
  State& owner = *entry->owner;
  OffsetGeometry geometry;
  if (!owner.offsetGeometry) return TypeError(cx, "Element layout port is not installed"), false;
  JS::RootedObject element(cx, &args.thisv().toObject());
  if (!owner.offsetGeometry(owner.offsetGeometryContext, element, &geometry)) return false;
  if (prop == OffsetParent) {
    if (!geometry.parent) return args.rval().setNull(), true;
    return owner.Wrap(geometry.parentKey, args.rval());
  }
  const double value = prop == OffsetWidth ? geometry.width : prop == OffsetHeight ? geometry.height
                     : prop == OffsetTop ? geometry.top : geometry.left;
  const double rounded = std::floor(value + .5);
  args.rval().setInt32(rounded >= INT32_MAX ? INT32_MAX : rounded <= INT32_MIN ? INT32_MIN : int32_t(rounded));
  return true;
}
bool ReflectedGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto prop = Prop(FunctionSlot(args));
  Wrapper* entry = HTMLReceiver(cx, args.thisv(), *FunctionState(args), &args);
  if (!entry) return false;
  State& state = *entry->owner;
  if (prop >= OffsetWidth) return OffsetGet(cx, args, prop, entry);
  if (prop == InnerText) {
    std::optional<DomString> text;
    if (!state.Check(state.model.TextContent(entry->key, &text))) return false;
    return Value(cx, text ? DomStringView(*text) : DomStringView(), args.rval());
  }
  std::optional<DomString> value;
  if (!state.Check(state.model.GetAttribute(entry->key, DomString(Attribute(prop)), &value)))
    return false;
  switch (prop) {
    case Dir:
      return Value(cx, domain::document_model::reflected_html_direction(
          value ? DomStringView(*value) : DomStringView()), args.rval());
    case Hidden: return args.rval().setBoolean(value.has_value()), true;
    case Draggable: return args.rval().setBoolean(value && *value == u"true"), true;
    case Spellcheck: return args.rval().setBoolean(!value || *value != u"false"), true;
    case TabIndex: return args.rval().setInt32(value ? ParseInt(*value) : 0), true;
    case IsContentEditable:
      return args.rval().setBoolean(value && (*value == u"true" || *value == u"plaintext-only")), true;
    case ContentEditable:
      return Value(cx, DomStringView(value ? Lower(*value) : DomString(u"inherit")), args.rval());
    default:
      return Value(cx, value ? DomStringView(*value) : DomStringView(), args.rval());
  }
}
bool ReflectedSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto prop = Prop(FunctionSlot(args));
  Wrapper* entry = HTMLReceiver(cx, args.thisv(), *FunctionState(args), &args);
  if (!entry) return false;
  args.rval().setUndefined();
  if (prop == InnerText) {
    DomString text;
    if (!String(cx, args.get(0), &text)) return false;
    State& state = *entry->owner;
    return state.Check(state.model.SetTextContent(entry->key, text));
  }
  if (prop == Hidden || prop == Draggable || prop == Spellcheck) {
    bool on = JS::ToBoolean(args.get(0));
    State& state = *entry->owner;
    DomString name(Attribute(prop));
    if (prop == Hidden && !on) return state.Check(state.model.RemoveAttribute(entry->key, name));
    DomString text = prop == Hidden ? DomString() : DomString(on ? u"true" : u"false");
    return state.Check(state.model.SetAttribute(entry->key, name, text));
  }
  if (prop == TabIndex) {
    int32_t value = 0;
    if (!JS::ToInt32(cx, args.get(0), &value)) return false;
    DomString text = Format(value);
    State& state = *entry->owner;
    return state.Check(state.model.SetAttribute(entry->key, u"tabindex", text));
  }
  DomString text;
  if (!String(cx, args.get(0), &text)) return false;
  DomString name(Attribute(prop));
  if (!text.empty() && prop == ContentEditable) text = Lower(text);
  State& state = *entry->owner;
  return state.Check(state.model.SetAttribute(entry->key, name, text));
}
}  // namespace

bool InstallHTMLElement(State& state) {
  if (!state.Accessor("HTMLElement", "style", StyleElementGetter, 0, StyleElementSetter)) return false;
  if (!state.Accessor("HTMLElement", "dataset", DatasetElementGetter, 0)) return false;
  static const struct { const char* name; Prop prop; bool writable; } props[] = {
    {"title", Title, true}, {"dir", Dir, true}, {"lang", Lang, true},
    {"accessKey", AccessKey, true}, {"contentEditable", ContentEditable, true},
    {"isContentEditable", IsContentEditable, false}, {"hidden", Hidden, true},
    {"draggable", Draggable, true}, {"spellcheck", Spellcheck, true},
    {"tabIndex", TabIndex, true}, {"innerText", InnerText, true},
    {"offsetWidth", OffsetWidth, false}, {"offsetHeight", OffsetHeight, false},
    {"offsetTop", OffsetTop, false}, {"offsetLeft", OffsetLeft, false},
    {"offsetParent", OffsetParent, false}
  };
  for (const auto& property : props)
    if (!state.Accessor("HTMLElement", property.name, ReflectedGet, property.prop,
                        property.writable ? ReflectedSet : nullptr)) return false;
  static const struct { const char* name; JSNative callback; } methods[] = {
    {"click", ClickElement}, {"focus", FocusMethod}, {"blur", BlurMethod}
  };
  for (const auto& method : methods)
    if (!state.Method("HTMLElement", method.name, method.callback, 0, 0)) return false;
  return InstallHTMLPresentation(state) && InstallForeignElementMembers(state);
}
}  // namespace ruyijs::document_model
