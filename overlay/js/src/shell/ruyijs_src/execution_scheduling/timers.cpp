#include "execution_scheduling/state.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/GCVector.h"
#include "js/Principals.h"
#include "js/String.h"
#include <new>

namespace ruyijs::execution_scheduling {
namespace {
// A string handler is dynamic code: as Firefox's CSPEvalChecker::CheckForWindow,
// ask the engine's one code-generation checker (the Content Security owner's
// hook, docs/owners/CONTENT_SECURITY.md) before the timer is registered.
bool StringHandlerAllowed(JSContext* cx, JS::HandleString code, bool* allowed) {
  *allowed = true;
  const JSSecurityCallbacks* callbacks = JS_GetSecurityCallbacks(cx);
  if (!callbacks || !callbacks->contentSecurityPolicyAllows) return true;
  JS::RootedVector<JSString*> parameters(cx);
  JS::RootedVector<JS::Value> arguments(cx);
  JS::RootedString body(cx);
  JS::RootedValue body_arg(cx);
  return callbacks->contentSecurityPolicyAllows(cx, JS::RuntimeCode::JS, code,
      JS::CompilationType::Undefined, parameters, body, arguments, body_arg, allowed);
}
}  // namespace
Callback* State::Find(uint64_t id) {
  for (Callback* cb = callbacks.get(); cb; cb = cb->next.get()) if (cb->id == id) return cb;
  return nullptr;
}
void State::Remove(uint64_t id) {
  auto* link = &callbacks;
  while (*link && (*link)->id != id) link = &(*link)->next;
  if (!*link || (*link)->running) return;
  argumentSlots -= (*link)->arguments.length();
  sourceUnits -= (*link)->sourceUnits;
  auto removed = std::move(*link);
  *link = std::move(removed->next);
}
State::RealmHandles& State::Handles(JSObject* global) {
  for (auto& handles : publicHandles) if (handles.global == global) return handles;
  publicHandles.push_back({global, {}, {}});
  return publicHandles.back();
}
void State::Cancel(uint32_t handle, bool frame, JSObject* global) {
  Callback* callback = callbacks.get();
  for (; callback; callback = callback->next.get()) {
    if (callback->global == global && callback->publicHandle == handle &&
        (frame ? callback->kind == Kind::Frame :
         callback->kind == Kind::Timeout || callback->kind == Kind::Interval)) break;
  }
  if (!callback) return;
  const uint64_t id = callback->id;
  // Public timer handles do not cancel posted messages or internal tasks.
  // Retirement uses Drop directly to cancel every task owned by that realm.
  model.Cancel(id, frame);
  if (!model.Exists(id)) Remove(id);
}
bool State::Register(std::unique_ptr<Callback> callback, int32_t delay,
                     JS::MutableHandleValue result) {
  // A retired document schedules nothing; the handle is never a live one.
  if (callback->global && Retired(callback->global)) { result.setInt32(0); return true; }
  SyncClock();
  if (callback->arguments.length() > 100000 - argumentSlots)
    return Error(cx, "Timer argument root limit exceeded", true);
  if (callback->sourceUnits > 16 * 1024 * 1024 - sourceUnits)
    return Error(cx, "Timer source limit exceeded", true);
  const bool frame = callback->kind == Kind::Frame;
  const bool publicId = frame || callback->kind == Kind::Timeout || callback->kind == Kind::Interval;
  domain::execution_scheduling::PublicHandleSequence* sequence = nullptr;
  if (publicId) {
    auto& handles = Handles(callback->global);
    sequence = frame ? &handles.frames : &handles.timers;
    callback->publicHandle = sequence->Reserve([&](uint32_t value) {
      for (Callback* live=callbacks.get(); live; live=live->next.get())
        if (live->global == callback->global && live->publicHandle == value &&
            (frame ? live->kind == Kind::Frame : live->kind == Kind::Timeout || live->kind == Kind::Interval)) return true;
      return false;
    });
    if (!callback->publicHandle) return Error(cx, "Public timer handles exhausted", true);
  }
  if (!Accept(model.Schedule(callback->kind, delay, &callback->id))) return false;
  if (sequence) sequence->Commit(callback->publicHandle);
  argumentSlots += callback->arguments.length();
  sourceUnits += callback->sourceUnits;
  result.setInt32(int32_t(publicId ? callback->publicHandle : 0));
  callback->next = std::move(callbacks);
  callbacks = std::move(callback);
  return true;
}
bool Timer(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  State* s = Receiver(cx, args);
  if (!s) return false;
  const bool interval = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32() != 0;
  s->Observe(s->GlobalInterface(), interval ? "setInterval" : "setTimeout", int(ApiAccess::Call));
  if (!argc) return Error(cx, "Timer requires a handler");
  JS::RootedValue handler(cx, args[0]);
  if (!handler.isObject() || !JS::IsCallable(&handler.toObject())) {
    JS::RootedString source(cx, JS::ToString(cx, handler));
    if (!source) return false;
    handler.setString(source);
    // A blocked string handler is not scheduled; the call returns 0.
    bool allowed = true;
    if (!StringHandlerAllowed(cx, source, &allowed)) return false;
    if (!allowed) { args.rval().setInt32(0); return true; }
  }
  int32_t delay = 0;
  if (argc > 1 && !JS::ToInt32(cx, args[1], &delay)) return false;
  const size_t slots = argc > 2 ? argc - 2 : 0;
  if (slots > 100000 - s->argumentSlots) return Error(cx, "Timer argument root limit exceeded", true);
  std::unique_ptr<Callback> callback(new (std::nothrow) Callback(cx));
  if (!callback) { JS_ReportOutOfMemory(cx); return false; }
  callback->kind = interval ? Kind::Interval : Kind::Timeout;
  callback->global = ReceiverGlobal(args);
  callback->handler = handler;
  callback->sourceUnits = handler.isString() ? JS_GetStringLength(handler.toString()) : 0;
  for (unsigned i = 2; i < argc; ++i) {
    if (!callback->arguments.append(args[i])) { JS_ReportOutOfMemory(cx); return false; }
  }
  return s->Register(std::move(callback), delay, args.rval());
}
bool ClearTimer(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  State* s = Receiver(cx, args);
  if (!s) return false;
  const bool interval = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32() != 0;
  s->Observe(s->GlobalInterface(), interval ? "clearInterval" : "clearTimeout", int(ApiAccess::Call));
  int32_t id = 0;
  if (argc && !JS::ToInt32(cx, args[0], &id)) return false;
  if (id > 0) s->Cancel(uint32_t(id), false, ReceiverGlobal(args));
  args.rval().setUndefined();
  return true;
}
}
