#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
enum Command { Exec, Enabled, Indeterm, StateQuery, Supported, ValueQuery, Clear };
constexpr const char* names[] = {"execCommand", "queryCommandEnabled", "queryCommandIndeterm",
    "queryCommandState", "queryCommandSupported", "queryCommandValue", "clear"};
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto method = static_cast<Command>(FunctionSlot(args));
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto* entry = DocumentReceiver(cx, object, &args);
  if (!entry) return false;
  entry->owner->Observe("Document", names[method], int(ApiAccess::Call));
  if (method == Clear) {
    if (!ClearDocument(*entry)) return false;
    args.rval().setUndefined(); return true;
  }
  if (!argc) return TypeError(cx, "Document command requires one argument");
  DomString command;
  if (!String(cx, args[0], &command)) return false;
  if (method == Exec && argc > 2) {
    JS::RootedString value(cx, JS::ToString(cx, args[2]));
    if (!value) return false;
  }
  if (method == ValueQuery) return Value(cx, DomStringView(), args.rval());
  args.rval().setBoolean(method == Supported && SupportedEditCommand(command));
  return true;
}
}
bool InstallDocumentCommands(State& state) {
  for (int i = Exec; i <= Clear; ++i)
    if (!state.Method("Document", names[i], Call, i == Clear ? 0 : 1, i)) return false;
  return true;
}
}
