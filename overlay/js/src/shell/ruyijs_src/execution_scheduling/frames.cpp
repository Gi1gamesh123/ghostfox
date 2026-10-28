#include "execution_scheduling/state.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include <new>

namespace ruyijs::execution_scheduling {
bool Frame(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  State* s = Receiver(cx, args);
  if (!s) return false;
  s->Observe(s->GlobalInterface(true), "requestAnimationFrame", int(ApiAccess::Call));
  if (!argc || !args[0].isObject() || !JS::IsCallable(&args[0].toObject()))
    return Error(cx, "requestAnimationFrame requires a callable callback");
  std::unique_ptr<Callback> callback(new (std::nothrow) Callback(cx));
  if (!callback) { JS_ReportOutOfMemory(cx); return false; }
  callback->kind = Kind::Frame;
  callback->global = ReceiverGlobal(args);
  callback->handler = args[0];
  return s->Register(std::move(callback), 0, args.rval());
}
bool ClearFrame(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  State* s = Receiver(cx, args);
  if (!s) return false;
  s->Observe(s->GlobalInterface(true), "cancelAnimationFrame", int(ApiAccess::Call));
  if (!argc) return Error(cx, "cancelAnimationFrame requires a handle");
  uint32_t id = 0;
  if (!JS::ToUint32(cx, args[0], &id)) return false;
  s->Cancel(id, true, ReceiverGlobal(args));
  args.rval().setUndefined();
  return true;
}
}
