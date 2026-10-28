#include "document_model/html_select.h"
#include "js/Conversions.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum Operation { Add, GetItem, NamedItem, Remove, Check, Report, SetValidity, ShowPicker };
const char* const kNames[] = {"add","item","namedItem","remove","checkValidity",
                             "reportValidity","setCustomValidity","showPicker"};
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int op = FunctionSlot(args);
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLSelectElement");
  if (!entry) return false;
  if (op == Add) return TypeError(cx, "Failed to execute 'add' on 'HTMLSelectElement': Parameter 1 is not of type 'HTMLOptionElement or HTMLOptGroupElement'");
  if ((op == GetItem || op == NamedItem || op == SetValidity) && argc == 0)
    return TypeError(cx, "Select method requires an argument");
  DomString text; uint32_t index = 0; int32_t removal = 0;
  if ((op == NamedItem || op == SetValidity) && !String(cx, args.get(0), &text)) return false;
  if (op == GetItem && !JS::ToUint32(cx, args.get(0), &index)) return false;
  if (op == Remove && argc && !JS::ToInt32(cx, args[0], &removal)) return false;
  auto& state = *entry->owner;
  bool valid = true;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLSelectElement", kNames[op], int(ApiAccess::Call));
    if (op == SetValidity && !state.Check(state.model.SetCustomValidity(entry->key, text))) return false;
    if (op == Check || op == Report) {
      bool enabled;
      if (!SelectWillValidate(*entry, &enabled) ||
          !state.Check(state.model.CustomValidity(entry->key, &text))) return false;
      valid = !enabled || text.empty();
    }
    if (op == Remove && argc == 0) {
      NodeSnapshot node;
      if (!state.Check(state.model.Inspect(entry->key, &node))) return false;
      if (!Same(node.parent, {}) && !state.Check(state.model.Remove(node.parent, entry->key))) return false;
    }
  }
  if (op == Check || op == Report) args.rval().setBoolean(valid);
  else if (op == GetItem || op == NamedItem) args.rval().setNull();
  else args.rval().setUndefined();
  return true;
}
}
bool InstallSelectMethods(State& state) {
  for (int i = 0; i < int(std::size(kNames)); ++i) {
    unsigned length = i == Add || i == GetItem || i == NamedItem || i == SetValidity ? 1 : 0;
    if (!state.Method("HTMLSelectElement", kNames[i], Call, length, i)) return false;
  }
  return true;
}
}
