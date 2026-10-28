#include "document_model/xpath.h"
#include "document_model/state.h"
#include "domain/document_model/xpath_values.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
enum Method { Expression, Resolver, Evaluate };
constexpr const char* names[] = {"createExpression", "createNSResolver", "evaluate"};
// Public expression/type coercion ignores ordinary errors; interrupts and OOM
// still propagate, and no source text is retained on the expression wrapper.
bool IgnoreConversionError(JSContext* cx) {
  if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
  JS_ClearPendingException(cx); return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const int slot = FunctionSlot(args);
  const int interface = slot / 3;
  const auto method = Method(slot % 3);
  JS::RootedObject receiver(cx);
  if (interface == 0) {
    receiver = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
    if (!DocumentReceiver(cx, receiver)) return false;
  } else if (!XPathReceiver(cx, args.thisv(),
      interface == 1 ? XPathKind::Evaluator : XPathKind::Expression, &receiver)) return false;
  state.Observe(interface == 0 ? "Document" : interface == 1 ? "XPathEvaluator" : "XPathExpression",
                names[method], int(ApiAccess::Call));
  const bool compiled = interface == 2;
  const unsigned required = method == Evaluate && !compiled ? 2 : 1;
  if (argc < required) return TypeError(cx, "XPath method requires more arguments");
  if (method != Resolver && !compiled) {
    JS::RootedString text(cx, JS::ToString(cx, args[0]));
    if (!text && !IgnoreConversionError(cx)) return false;
  }
  if (method == Expression) return NewXPath(state, XPathKind::Expression, 0, args.rval());
  const unsigned nodeIndex = method == Resolver || compiled ? 0 : 1;
  if (!args[nodeIndex].isObject()) return TypeError(cx, "XPath context must be an object");
  if (method == Resolver) { args.rval().set(args[0]); return true; }
  const unsigned typeIndex = compiled ? 1 : 3;
  uint16_t type = 0;
  if (!JS::ToUint16(cx, args.get(typeIndex), &type)) {
    if (!IgnoreConversionError(cx)) return false;
    type = 0;
  }
  return NewXPath(state, XPathKind::Result,
      ruyijs::domain::document_model::xpath_result_type(type), args.rval());
}
}
bool InstallXPath(State& state) {
  for (int iface = 0; iface < 2; ++iface)
    for (int method = Expression; method <= Evaluate; ++method)
      if (!state.Method(iface == 0 ? "Document" : "XPathEvaluator", names[method], Call,
                        method == Evaluate ? 2 : 1, iface * 3 + method)) return false;
  return state.Method("XPathExpression", "evaluate", Call, 1, 2 * 3 + Evaluate) &&
      InstallXPathResult(state);
}
}
