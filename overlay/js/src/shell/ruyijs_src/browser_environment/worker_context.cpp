#include "browser_environment/state.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs::browser_environment {
namespace {
constexpr const char* kNames[] = {"origin", "isSecureContext", "crossOriginIsolated"};
State* Receiver(JSContext* cx, const JS::CallArgs& args, unsigned* slot) {
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  *slot = unsigned(js::GetFunctionNativeReserved(&args.callee(), 1).toInt32());
  if (state && state->worker && (args.thisv().isNullOrUndefined() ||
      (args.thisv().isObject() && &args.thisv().toObject() == state->page))) return state;
  Error(cx, "Worker context called on incompatible receiver"); return nullptr;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  unsigned slot;
  State* state = Receiver(cx, args, &slot);
  if (!state) return false;
  state->Observe("WorkerGlobalScope", kNames[slot], int(ApiAccess::Read));
  if (!slot) return state->ReadOrigin(args.rval());
  args.rval().setBoolean(slot == 1 ? state->workerContext.secure : state->IsCrossOriginIsolated());
  return true;
}
bool SetOrigin(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  unsigned slot;
  State* state = Receiver(cx, args, &slot);
  if (!state) return false;
  state->Observe("WorkerGlobalScope", "origin", int(ApiAccess::Write));
  if (!JS_DefineProperty(cx, state->page, "origin", args.get(0), JSPROP_ENUMERATE)) return false;
  args.rval().setUndefined(); return true;
}
}
bool State::InstallWorkerContext(JS::HandleObject prototype) {
  for (unsigned slot = 0; slot < 3; ++slot)
    if (!Accessor(prototype, kNames[slot], slot, JSPROP_ENUMERATE, Get, slot ? nullptr : SetOrigin) ||
        !helpers->Accessor("WorkerGlobalScope", prototype, kNames[slot])) return false;
  return true;
}
bool State::ExportWorkerContext(JS::HandleObject output) {
  JS::RootedObject context(cx, JS_NewPlainObject(cx));
  JS::RootedValue origin(cx), secureValue(cx), isolatedValue(cx, JS::BooleanValue(IsCrossOriginIsolated()));
  bool secure;
  if (!context || !ReadOrigin(&origin) || !ReadSecureContext(&secure)) return false;
  secureValue.setBoolean(secure);
  return JS_DefineProperty(cx, context, "origin", origin, JSPROP_ENUMERATE) &&
      JS_DefineProperty(cx, context, "secure", secureValue, JSPROP_ENUMERATE) &&
      JS_DefineProperty(cx, context, "isolated", isolatedValue, JSPROP_ENUMERATE) &&
      JS_DefineProperty(cx, output, "workerContext", context, JSPROP_ENUMERATE);
}
bool State::ApplyWorkerContext(JS::HandleObject input) {
  JS::RootedValue value(cx), origin(cx), secure(cx), isolated(cx);
  if (!JS_GetProperty(cx, input, "workerContext", &value)) return false;
  if (!value.isObject()) return Error(cx, "Worker context snapshot is missing");
  JS::RootedObject object(cx, &value.toObject());
  JS::Rooted<JS::IdVector> keys(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, object, &keys) ||
      !JS_GetProperty(cx, object, "origin", &origin) || !JS_GetProperty(cx, object, "secure", &secure) ||
      !JS_GetProperty(cx, object, "isolated", &isolated)) return false;
  if (keys.length() != 3 || !origin.isString() || !secure.isBoolean() || !isolated.isBoolean())
    return Error(cx, "Invalid Worker context snapshot");
  JS::RootedString text(cx, origin.toString());
  std::string source;
  if (!ToUTF8(cx, text, &source)) return false;
  OriginValue typed;
  if (!Model::ReadUrlOrigin(source, &typed)) { JS_ReportOutOfMemory(cx); return false; }
  if (typed.text() != source)
    return Error(cx, "Invalid Worker context origin");
  workerContext = {std::move(typed), secure.toBoolean(), isolated.toBoolean()};
  return JS_DeleteProperty(cx, input, "workerContext");
}
}
