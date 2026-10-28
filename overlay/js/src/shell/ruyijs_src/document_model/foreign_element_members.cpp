// Foreign elements share the original DOM attribute and reflected-object owners.
#include "document_model/state.h"
#include "document_model/html_element_internal.h"
#include "document_model/style_internal.h"
#include "domain/document_model/tab_index_value.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum Member { Style, Dataset, TabIndex, Id, ClassName };
Wrapper* Receiver(JSContext* cx, const JS::CallArgs& args) {
  auto* entry = ElementReceiver(cx, args.thisv());
  if (!entry) return nullptr;
  NodeSnapshot node;
  if (!FunctionState(args)->Check(entry->owner->model.Inspect(entry->key, &node))) return nullptr;
  const auto space = FunctionSlot(args) >= 8 ? u"http://www.w3.org/1998/Math/MathML" : u"http://www.w3.org/2000/svg";
  if (node.namespace_uri && *node.namespace_uri == space) return entry;
  TypeError(cx, "Foreign element receiver is incompatible"); return nullptr;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* entry = Receiver(cx, args);
  if (!entry) return false;
  const auto member = Member(FunctionSlot(args) % 8);
  if (member == Style) return StyleElementGetter(cx, argc, vp);
  if (member == Dataset) return DatasetElementGetter(cx, argc, vp);
  if (member == ClassName) return AnimatedClassObject(*entry, args.rval());
  std::optional<DomString> value;
  if (!FunctionState(args)->Check(entry->owner->model.GetAttribute(entry->key,
      member == Id ? u"id" : u"tabindex", &value))) return false;
  if (member == TabIndex) {
    args.rval().setInt32(value ? ruyijs::domain::document_model::tab_index_value(*value) : -1);
    return true;
  }
  return Value(cx, DomStringView(value ? *value : DomString()), args.rval());
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* entry = Receiver(cx, args);
  if (!entry) return false;
  const auto member = Member(FunctionSlot(args) % 8);
  if (member == Style) {
    JS::RootedObject node(cx, entry->object), style(cx);
    { JSAutoRealm realm(cx, entry->owner->page);
      if (!StyleObject(*entry->owner, node, &style)) return false; }
    if (!JS_WrapObject(cx, &style) || !JS_SetProperty(cx, style, "cssText", args.get(0))) return false;
  } else {
    DomString text;
    if (member == TabIndex) {
      int32_t value;
      if (!JS::ToInt32(cx, args.get(0), &value)) return false;
      const auto digits = std::to_string(value); text.assign(digits.begin(), digits.end());
    } else if (!String(cx, args.get(0), &text)) return false;
    if (!FunctionState(args)->Check(entry->owner->model.SetAttribute(entry->key,
        member == Id ? u"id" : u"tabindex", text))) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallForeignElementMembers(State& state) {
  for (int kind = 0; kind < 2; ++kind) {
    const auto* name = kind ? "MathMLElement" : "SVGElement";
    if (!state.Accessor(name, "style", Get, kind * 8 + Style, Set) ||
        !state.Accessor(name, "dataset", Get, kind * 8 + Dataset) ||
        !state.Accessor(name, "tabIndex", Get, kind * 8 + TabIndex, Set)) return false;
  }
  return state.Accessor("SVGElement", "id", Get, Id, Set) &&
      state.Accessor("SVGElement", "className", Get, ClassName) && InstallAnimatedString(state);
}
}
