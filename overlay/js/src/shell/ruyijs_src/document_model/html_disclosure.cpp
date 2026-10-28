#include "document_model/html_control.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum Kind { Open, Name, ReturnValue, Dormant };
struct Property { const char* iface; const char* name; Kind kind; };
const Property kProperties[] = {
  {"HTMLDetailsElement", "open", Open}, {"HTMLDetailsElement", "name", Name},
  {"HTMLDialogElement", "open", Open}, {"HTMLDialogElement", "returnValue", ReturnValue},
  {"HTMLSlotElement", "name", Dormant}
};
// Public disclosure string setters consume ordinary coercion errors, but the
// engine's OOM and interrupt paths must still propagate to the runtime.
bool DisclosureString(JSContext* cx, JS::HandleValue value, DomString* text) {
  JS::RootedString converted(cx, JS::ToString(cx, value));
  if (!converted) {
    if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
    JS_ClearPendingException(cx); text->clear(); return true;
  }
  // Product string/storage limits are not user conversion exceptions.
  JS::RootedValue string(cx, JS::StringValue(converted));
  return String(cx, string, text);
}
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& p = kProperties[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), p.iface);
  if (!entry) return false;
  auto& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe(p.iface, p.name, int(ApiAccess::Read));
    if (p.kind == Dormant) return TypeError(cx, "Public slot member has no initialized implementation");
    std::optional<DomString> attr; DomString text;
    if (p.kind == ReturnValue) {
      if (!state.Check(state.model.DialogReturnValue(entry->key, &text))) return false;
    } else {
      if (!state.Check(state.model.GetAttribute(entry->key, p.kind == Open ? u"open" : u"name", &attr))) return false;
      text = attr.value_or(u"");
    }
    if (p.kind == Open) result.setBoolean(attr.has_value());
    else if (!Value(cx, DomStringView(text), &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Write(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& p = kProperties[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), p.iface);
  if (!entry) return false;
  if (p.kind == Dormant) return TypeError(cx, "Public slot member has no initialized implementation");
  DomString text;
  if (p.kind != Open && !DisclosureString(cx, args.get(0), &text)) return false;
  const bool present = JS::ToBoolean(args.get(0));
  auto& state = *entry->owner;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe(p.iface, p.name, int(ApiAccess::Write));
    auto status = p.kind == ReturnValue ? state.model.SetDialogReturnValue(entry->key, text)
        : p.kind == Name ? state.model.SetAttribute(entry->key, u"name", text)
        : present ? state.model.SetAttribute(entry->key, u"open", u"")
                  : state.model.RemoveAttribute(entry->key, u"open");
    if (!state.Check(status)) return false;
  }
  args.rval().setUndefined(); return true;
}
bool Dialog(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int action = FunctionSlot(args);
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLDialogElement");
  if (!entry) return false;
  std::optional<DomString> value;
  if (action == 2 && !args.get(0).isUndefined()) {
    value.emplace();
    if (!DisclosureString(cx, args.get(0), &*value)) return false;
  }
  auto& state = *entry->owner;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLDialogElement", action == 0 ? "show" : action == 1 ? "showModal" : "close", int(ApiAccess::Call));
    if (!state.Check(state.model.DialogAction(entry->key, action == 2, value))) return false;
  }
  args.rval().setUndefined(); return true;
}
bool Slot(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!ControlReceiver(cx, args.thisv(), "HTMLSlotElement")) return false;
  return TypeError(cx, "Public slot member has no initialized implementation");
}
}
bool InstallDisclosureElements(State& state) {
  for (size_t i = 0; i < std::size(kProperties); ++i) {
    const auto& p = kProperties[i];
    if (!state.Accessor(p.iface, p.name, Read, int32_t(i), Write)) return false;
  }
  return state.Method("HTMLDialogElement", "show", Dialog, 0, 0) &&
      state.Method("HTMLDialogElement", "showModal", Dialog, 0, 1) &&
      state.Method("HTMLDialogElement", "close", Dialog, 0, 2) &&
      state.Method("HTMLSlotElement", "assignedNodes", Slot, 0, 0) &&
      state.Method("HTMLSlotElement", "assignedElements", Slot, 0, 1) &&
      state.Method("HTMLSlotElement", "assign", Slot, 0, 2);
}
}
