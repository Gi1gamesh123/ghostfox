// Shared Body binding validation and the original one-shot consumption boundary.
#include "resource_network/body.h"
#include "resource_network/state.h"
#include "js/ErrorReport.h"
#include "js/Object.h"
#include <cstring>

namespace ruyijs::resource_network {
namespace {
const JSErrorFormatString* BodyErrorFormat(void* data, unsigned) {
  return static_cast<const JSErrorFormatString*>(data);
}
}
bool BodyOperationError(JSContext* cx, const char* iface, const char* method,
                        bool invalidReceiver) {
  JSErrorFormatString format{"RUYIJS_BODY_OPERATION", invalidReceiver
      ? "'{1}' called on an object that does not implement interface {0}."
      : "{0}.{1}: Body has already been consumed.", 2, JSEXN_TYPEERR};
  JS_ReportErrorNumberASCII(cx, BodyErrorFormat, &format, 0, iface, method);
  return false;
}
bool ConsumeBody(JSContext* cx, const JS::CallArgs& args, const char* method,
                 BodyRecord** body, bool* consumed) {
  const char* iface = BodyInterface(args);
  JS::RootedObject receiver(cx, BodyReceiver(args.thisv()));
  const JSClass* expected = std::strcmp(iface, "Response") == 0
      ? &kResponseClass : &kRequestClass;
  const bool invalid = !receiver || JS::GetClass(receiver) != expected;
  *body = invalid ? nullptr : BodyOf(args.thisv());
  // This flag also stops the caller after a binding-error rejection.
  *consumed = invalid || BodyUsed(BodyHolderOf(args.thisv())) || BodyLocked(BodyHolderOf(args.thisv()));
  if (*consumed) {
    BodyOperationError(cx, iface, method, invalid);
    JS::RootedValue error(cx);
    if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx, &error)) return false;
    JS_ClearPendingException(cx);
    return Rejected(cx, error, args.rval());
  }
  return (*body)->source!=BodySource::OwnBytes || BeginBodyConsume(cx,args.thisv());
}
}
