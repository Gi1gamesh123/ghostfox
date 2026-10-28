#include "document_model/html_input.h"
#include "js/Conversions.h"
#include "js/Realm.h"
#include <cmath>

namespace ruyijs::document_model {
namespace {
enum Method { StepUp, StepDown, Check, Report, Custom, Select, ReplaceRange, Selection, Picker };
const char* const kNames[] = {"stepUp","stepDown","checkValidity","reportValidity","setCustomValidity",
    "select","setRangeText","setSelectionRange","showPicker"};
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int method = FunctionSlot(args);
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLInputElement");
  if (!entry) return false;
  auto& state = *entry->owner;
  DomString text; int32_t start = 0, end = 0; double step = 1;
  if ((method == Custom || method == ReplaceRange) && argc && !ControlString(cx, args[0], &text)) return false;
  if (method == Selection && argc >= 2) {
    if (argc && !JS::ToInt32(cx, args[0], &start)) return false;
    if (argc > 1 && !JS::ToInt32(cx, args[1], &end)) return false;
    if (argc > 2 && !ControlString(cx, args[2], &text)) return false;
  }
  if (method == ReplaceRange && argc >= 4) {
    if (!JS::ToInt32(cx, args[1], &start) || !JS::ToInt32(cx, args[2], &end)) return false;
  }
  if ((method == StepUp || method == StepDown) && argc && args[0].isNumber()) {
    const double number = args[0].toNumber();
    if (std::isfinite(number) && std::trunc(number) == number && number >= INT32_MIN && number <= INT32_MAX) step = number;
  }
  uint32_t flags = 0;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLInputElement", kNames[method], int(ApiAccess::Call));
    if (method == Check || method == Report) {
      if (!state.Check(state.model.ControlValidation(entry->key, &flags))) return false;
    } else if (method == Custom) {
      if (!state.Check(state.model.SetCustomValidity(entry->key, text))) return false;
    } else if (method == ReplaceRange && argc) {
      if (!state.Check(state.model.InputRange(entry->key, text, start, end, argc >= 4))) return false;
    } else if (method == StepUp || method == StepDown) {
      if (!state.Check(state.model.SetInputNumber(entry->key, method == StepUp ? step : -step, true))) return false;
    }
  }
  if (method == Check || method == Report) args.rval().setBoolean(!flags);
  else args.rval().setUndefined();
  return true;
}
}
bool InstallInputMethods(State& state) {
  for (int i = 0; i < int(std::size(kNames)); ++i)
    if (!state.Method("HTMLInputElement", kNames[i], Call, i == Custom || i == ReplaceRange ? 1 : i == Selection ? 2 : 0, i)) return false;
  return true;
}
}
