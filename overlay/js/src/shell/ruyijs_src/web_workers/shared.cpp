// SharedWorker: the Window-side constructor, `port` accessor and the Context's
// one process registry keyed by (url, name) (docs/owners/WORKER.md, "Shared
// workers"). Connections reuse the Web Workers process and pipe; the ports are
// web_messaging remote ports the runtime binds to this owner.
#include "web_workers/state.h"
#include "web_workers/handlers.h"
#include "web_workers/options.h"
#include "web_workers/bootstrap.h"
#include "web_workers/clone.h"
#include "web_workers/error_event.h"
#include "document_model/exception.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/runtime.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/StableStringChars.h"
#include "js/String.h"
#include "js/Wrapper.h"

namespace ruyijs::web_workers {
namespace {
void FinalizeSharedWorker(JS::GCContext*, JSObject* object) {
  auto slot = JS::GetReservedSlot(object, 0);
  if (!slot.isUndefined()) delete static_cast<SharedWorkerRecord*>(slot.toPrivate());
}
const JSClassOps kSharedWorkerOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                     FinalizeSharedWorker};
bool UrlText(JSContext* cx, JS::HandleValue value, std::string* text) {
  JS::RootedString string(cx, JS::ToString(cx, value));
  return string && ToUTF8(cx, string, text);
}
// SharedWorker's second argument is (DOMString or WorkerOptions): a plain
// string is the name; an object reads name/credentials/type as Worker does.
bool ReadSharedWorkerName(JSContext* cx, JS::HandleValue input, std::u16string* name) {
  if (input.isNullOrUndefined()) return true;
  if (input.isString()) {
    JS::RootedString string(cx, input.toString());
    JS::AutoStableStringChars chars(cx);
    if (!chars.initTwoByte(cx, string)) return false;
    if (chars.length() > 65536) return WebKit::Fail(cx, "SharedWorker name limit exceeded");
    name->assign(chars.twoByteChars(), chars.length());
    return true;
  }
  return ReadWorkerName(cx, input, name);
}
// A refused (blocked) script's task: the SharedWorker's plain `error` event.
bool FireRefusedShared(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject task(cx, &args.callee());
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(task, 0).toPrivate());
  JS::RootedObject target(cx, &js::GetFunctionNativeReserved(task, 1).toObject());
  args.rval().setUndefined();
  return state->events->Fire(target, "error", false, false);
}
}  // namespace
// The origin State of this Context's worker group: the first realm joined. It
// stays alive for the Context's lifetime (predecessors are kept), so the
// Context-wide SharedWorker registry lives there.
State& SharedOwner(State& state) {
  auto& group = state.budget->group;
  return group.empty() ? state : *group[0];
}
const JSClass kSharedWorkerClass = {"SharedWorker", JSCLASS_HAS_RESERVED_SLOTS(3) |
                                    JSCLASS_FOREGROUND_FINALIZE, &kSharedWorkerOps};

bool SharedWorkerPort(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx, args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!self || JS::GetClass(self) != &kSharedWorkerClass)
    return WebKit::Fail(cx, "SharedWorker port receiver is invalid");
  args.rval().set(JS::GetReservedSlot(self, 2));
  return JS_WrapValue(cx, args.rval());
}

bool SharedWorkerConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* state = static_cast<State*>(WebKit::Owner(args));
  NativeConstructorTrace trace(cx, args);
  if (!state || !args.isConstructing() || !argc)
    return WebKit::Fail(cx, "SharedWorker requires a script URL");
  State& owner = SharedOwner(*state);
  owner.Prune();
  if (!owner.task) return WebKit::Fail(cx, "Worker task port is not installed");
  JS::RootedObject proto(cx);
  if (!state->kit.ConstructProto(args, state->sharedWorker.prototype, &proto)) return false;
  std::string url;
  std::u16string name;
  if (!UrlText(cx, args[0], &url) || !ReadSharedWorkerName(cx, args.get(1), &name)) return false;
  // The same URL resolution, origin check and Blob loading as the Worker
  // constructor. A Blob URL carries its script in the startup frame; any other
  // same-origin URL is fetched by the shared worker's own network owner.
  std::string script;
  bool blocked = false;
  if (url.rfind("blob:", 0) == 0) {
    std::string type;
    if (!state->objectUrl || !state->objectUrl(state->objectUrlContext, url, &script, &type))
      return WebKit::Fail(cx, "SharedWorker script URL could not be loaded");
  } else {
    std::string resolved;
    bool sameOrigin = false;
    JS::RootedObject exception(cx, state->exceptionPrototype);
    if (!state->scriptUrl || !state->scriptUrl(state->scriptUrlContext, url, &resolved, &sameOrigin)) {
      if (JS_IsExceptionPending(cx)) return false;
      return document_model::ThrowException(cx, exception, "SyntaxError",
          ("SharedWorker constructor: Failed to load worker script at \"" + url + "\"").c_str());
    }
    const bool network = resolved.rfind("http:", 0) == 0 || resolved.rfind("https:", 0) == 0;
    blocked = resolved.rfind("data:", 0) != 0 && !(network && sameOrigin);
    url = std::move(resolved);
  }
  // The port exists even for a refused script; its relay just drops messages.
  if (!state->portFactory) return WebKit::Fail(cx, "SharedWorker port factory is not bound");
  SharedWorkerProcess* process = nullptr;
  for (auto& entry : owner.sharedWorkers)
    if (entry->url == url && entry->name == name) { process = entry.get(); break; }
  bool started = false;
  if (!blocked && !process) {
    if (!state->environment) return WebKit::Fail(cx, "Worker environment port is not installed");
    std::string bootstrap, environment;
    if (!state->environment(state->environmentContext, &environment)) return false;
    if (!EncodeStartup(url, script, &bootstrap, name, environment))
      return WebKit::Fail(cx, "SharedWorker startup frame limit exceeded");
    const std::wstring executable = WorkerExecutable();
    if (executable.empty()) return WebKit::Fail(cx, "Worker executable is unavailable");
    if (owner.budget->active >= Budget::kLimit)
      return WebKit::Fail(cx, "Active Worker limit exceeded");
    auto entry = std::make_unique<SharedWorkerProcess>(&owner, owner.budget);
    entry->url = url;
    entry->name = name;
    if (owner.serviceFactory && !owner.serviceFactory(owner.serviceContext, &entry->record.service)) return false;
    if (!entry->record.process.Start(executable, std::move(bootstrap), WorkerKind::Shared)) {
      entry->record.Stop();
      return WebKit::Fail(cx, "SharedWorker could not be started");
    }
    process = entry.get();
    owner.sharedWorkers.push_back(std::move(entry));
    started = true;
  }
  const uint32_t id = ++owner.budget->nextSharedPort;
  JS::RootedObject port(cx, state->portFactory(state->portFactoryContext, id));
  auto* record = new(std::nothrow) SharedWorkerRecord(state);
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kSharedWorkerClass, proto));
  if (!record || !object || !port) { delete record; JS_ReportOutOfMemory(cx); return false; }
  record->id = id;
  JS::SetReservedSlot(object, 0, JS::PrivateValue(record));
  JS::RootedObject handlers(cx, NewHandlerStore(cx, state->handlerSource));
  if (!handlers) return false;
  JS::SetReservedSlot(object, 1, JS::ObjectValue(*handlers));
  JS::SetReservedSlot(object, 2, JS::ObjectValue(*port));
  if (!state->events->Attach(object)) return false;
  if (process) {
    SharedConnection connection{id, state, object.get()};
    process->connections.push_back(connection);
    if (!owner.sharedObjects.append(object) || !owner.Schedule()) return false;
  }
  if (blocked) {
    // A refused script has no process: one queued `error` event, as Worker.
    JSFunction* fn = js::NewFunctionWithReserved(cx, FireRefusedShared, 0, 0, "SharedWorker error");
    if (!fn) return false;
    JS::RootedObject fire(cx, JS_GetFunctionObject(fn));
    js::SetFunctionNativeReserved(fire, 0, JS::PrivateValue(state));
    js::SetFunctionNativeReserved(fire, 1, JS::ObjectValue(*object));
    if (!state->task(state->taskContext, fire, 0)) return false;
  } else if (started) {
    std::string connect;
    EncodeSharedPortFrame(id, kSharedConnect, &connect);
    if (!process->record.process.Queue(std::move(connect))) return false;
  } else {
    std::string connect;
    EncodeSharedPortFrame(id, kSharedConnect, &connect);
    if (!process->record.process.Queue(std::move(connect))) return false;
  }
  args.rval().setObject(*object);
  return trace.Finish(true);
}
}  // namespace ruyijs::web_workers
