#include "document_model/html_textarea.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum Method { Check, Report, Select, Custom, ReplaceRange, Selection };
const char* const kNames[] = {"checkValidity","reportValidity","select","setCustomValidity",
    "setRangeText","setSelectionRange"};
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int method = FunctionSlot(args);
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLTextAreaElement");
  if (!entry) return false;
  const unsigned required = method == Selection ? 2 : method == Custom || method == ReplaceRange ? 1 : 0;
  if (argc < required) return TypeError(cx, "Textarea method requires arguments");
  auto& state = *entry->owner;
  DomString text, direction; uint32_t start = 0, end = 0;
  if ((method == Custom || method == ReplaceRange) && !ControlString(cx, args[0], &text)) return false;
  if (method == Selection || (method == ReplaceRange && argc >= 3)) {
    const int offset = method == Selection ? 0 : 1;
    if (!JS::ToUint32(cx, args[offset], &start) || !JS::ToUint32(cx, args[offset + 1], &end)) return false;
    if (argc > unsigned(offset + 2) && !ControlString(cx, args[offset + 2], &direction)) return false;
  }
  uint32_t flags = 0;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLTextAreaElement", kNames[method], int(ApiAccess::Call));
    if (method == Check || method == Report) {
      if (!state.Check(state.model.ControlValidation(entry->key, &flags))) return false;
    } else if (method == Custom) {
      if (!state.Check(state.model.SetCustomValidity(entry->key, text))) return false;
    } else if (method == ReplaceRange) {
      if (!state.Check(state.model.TextareaRange(entry->key, text, start, end, argc >= 3, direction))) return false;
    } else if (!state.Check(state.model.SetTextareaSelection(entry->key, start, end, direction,
        method == Select ? 4 : 3))) return false;
  }
  if (method == Check || method == Report) args.rval().setBoolean(!flags);
  else args.rval().setUndefined();
  return true;
}
bool LabelItem(JSContext*, unsigned argc, JS::Value* vp) {
  JS::CallArgsFromVp(argc, vp).rval().setNull(); return true;
}
}
bool TextareaLabels(State& state, JS::MutableHandleValue out) {
  JS::RootedObject array(state.cx, JS::NewArrayObject(state.cx, 0));
  JS::RootedObject item(state.cx, state.Function(LabelItem, "item", 0, 0));
  if (!array || !item || !JS_DefineProperty(state.cx, array, "item", item, JSPROP_ENUMERATE)) return false;
  out.setObject(*array); return true;
}
bool InstallTextareaMethods(State& state) {
  for (int i = 0; i < int(std::size(kNames)); ++i)
    if (!state.Method("HTMLTextAreaElement", kNames[i], Call,
        i == Selection ? 2 : i == Custom || i == ReplaceRange ? 1 : 0, i)) return false;
  return true;
}
}
