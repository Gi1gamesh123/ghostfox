#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/Exception.h"

namespace ruyijs::document_model {
namespace {
constexpr const char* methods[] = {"getHTML", "setHTML", "setHTMLUnsafe"};
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& function = *FunctionState(args);
  const int method = FunctionSlot(args);
  if (!ElementReceiver(cx, args.thisv())) return false;
  function.Observe("Element", methods[method], int(ApiAccess::Call));
  if (!method) return Value(cx, DomStringView(u"<div></div>"), args.rval());
  // The public setters consume a string conversion but neither parse it nor
  // read options. Ordinary conversion exceptions are swallowed by that tool.
  if (argc) {
    JS::RootedString ignored(cx, JS::ToString(cx, args[0]));
    if (!ignored) {
      if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
      JS_ClearPendingException(cx);
    }
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallElementHTML(State& state) {
  for (int i = 0; i < int(std::size(methods)); ++i)
    if (!state.Method("Element", methods[i], Call, i ? 1 : 0, i)) return false;
  return true;
}
}
