#include "document_model/html_table.h"
#include "js/Conversions.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum Kind { Text, Legacy, Span, Boolean };
struct Property { TableBrand brand; const char* name; const char16_t* attr; Kind kind=Text; };
const Property kProperties[]={
  {Table,"align",u"align"},{Table,"border",u"border"},{Table,"frame",u"frame"},
  {Table,"rules",u"rules"},{Table,"summary",u"summary"},{Table,"width",u"width"},
  {Table,"bgColor",u"bgcolor",Legacy},{Table,"cellPadding",u"cellpadding",Legacy},
  {Table,"cellSpacing",u"cellspacing",Legacy},
  {Section,"align",u"align"},{Section,"ch",u"char"},{Section,"chOff",u"charoff"},
  {Section,"vAlign",u"valign"},
  {Row,"align",u"align"},{Row,"ch",u"char"},{Row,"chOff",u"charoff"},
  {Row,"vAlign",u"valign"},{Row,"bgColor",u"bgcolor",Legacy},
  {Cell,"colSpan",u"colspan",Span},{Cell,"rowSpan",u"rowspan",Span},
  {Cell,"headers",u"headers"},{Cell,"abbr",u"abbr"},{Cell,"scope",u"scope"},
  {Cell,"align",u"align"},{Cell,"axis",u"axis"},{Cell,"height",u"height"},
  {Cell,"width",u"width"},{Cell,"ch",u"char"},{Cell,"chOff",u"charoff"},
  {Cell,"noWrap",u"nowrap",Boolean},{Cell,"vAlign",u"valign"},{Cell,"bgColor",u"bgcolor",Legacy},
  {Col,"span",u"span",Span},{Col,"align",u"align"},{Col,"ch",u"char"},
  {Col,"chOff",u"charoff"},{Col,"vAlign",u"valign"},{Col,"width",u"width"},
  {Caption,"align",u"align"}
};
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  const auto& p=kProperties[FunctionSlot(args)];
  auto* entry=TableReceiver(cx,args,p.brand);
  if (!entry) return false;
  auto& state=*entry->owner;
  DomString text; uint32_t number=1; bool present=false;
  {
    JSAutoRealm realm(cx,state.page);
    state.Observe(TableInterface(p.brand),p.name,int(ApiAccess::Read));
    if (p.kind==Span) {
      if (!state.Check(state.model.TableSpan(entry->key,p.attr,&number))) return false;
    } else if (p.kind==Boolean) {
      std::optional<DomString> raw;
      if (!state.Check(state.model.GetAttribute(entry->key,p.attr,&raw))) return false;
      present=raw.has_value();
    } else if (!state.Check(state.model.TableAttribute(entry->key,p.attr,&text))) return false;
  }
  if (p.kind==Span) args.rval().setNumber(number);
  else if (p.kind==Boolean) args.rval().setBoolean(present);
  else return Value(cx,DomStringView(text),args.rval());
  return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  const auto& p=kProperties[FunctionSlot(args)];
  auto* entry=TableReceiver(cx,args,p.brand);
  if (!entry) return false;
  DomString text;
  bool present=true;
  if (p.kind==Boolean) present=JS::ToBoolean(args.get(0));
  else if (p.kind==Span) {
    uint32_t n;
    if (!JS::ToUint32(cx,args.get(0),&n)) return false;
    std::string ascii=std::to_string(n>2147483647u ? 1 : n);
    text.assign(ascii.begin(),ascii.end());
  } else if (!(p.kind==Legacy && args.get(0).isNull()) && !String(cx,args.get(0),&text)) return false;
  auto& state=*entry->owner;
  {
    JSAutoRealm realm(cx,state.page);
    state.Observe(TableInterface(p.brand),p.name,int(ApiAccess::Write));
    if (!state.Check(present ? state.model.SetAttribute(entry->key,p.attr,text) :
        state.model.RemoveAttribute(entry->key,p.attr))) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallTableAttributes(State& state) {
  for (size_t i=0;i<std::size(kProperties);++i) {
    const auto& p=kProperties[i];
    if (!state.Accessor(TableInterface(p.brand),p.name,Get,int32_t(i),Set)) return false;
  }
  return true;
}
}
