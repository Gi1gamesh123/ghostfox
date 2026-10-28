#include "browser_environment/state.h"
#include "browser_environment/storage_manager.h"
#include "browser_environment/storage_opfs.h"
#include "browser_environment/string_list.h"
#include "domain/browser_environment/url_value.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/Realm.h"

namespace ruyijs::browser_environment {
namespace {
bool WorkerGlobalGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state || !state->worker || (!args.thisv().isNullOrUndefined() &&
      (!args.thisv().isObject() || &args.thisv().toObject() != state->page)))
    return Error(cx, "Worker environment called on incompatible receiver");
  const auto kind = Kind(js::GetFunctionNativeReserved(&args.callee(), 1).toInt32());
  state->Observe("WorkerGlobalScope", kind == Kind::Location ? "location" : "navigator", int(ApiAccess::Read));
  args.rval().setObject(*state->instances.get()[int32_t(kind)]);
  return true;
}
}

bool Environment::InstallFileSystem() {
  JSAutoRealm realm(state_->cx, state_->page);
  return InstallOpfs(*state_);
}

bool Environment::InstallWorker(PageHelpers& helpers, JS::HandleObject globalScopePrototype,
    const std::string& scriptUrl, std::string_view profile, bool dedicated) {
  State& state = *state_;
  if (state.installed) return Error(state.cx, "Environment is already installed");
  using ruyijs::domain::browser_environment::UrlValue;
  UrlValue url;
  if (!UrlValue::Resolve(scriptUrl, "", &url) || !url.parsed())
    return Error(state.cx, "Worker location requires an absolute script URL");
  state.helpers = &helpers;
  state.worker = true;
  state.dedicatedWorker = dedicated;
  state.frameUrl = url.href();
  state.workerContext = {url.origin_value(),
      ruyijs::domain::browser_environment::TrustworthyContextUrl(url), false};
  if (!state.ApplyWorkerProfile(profile)) return false;
  state.ready = true;
  JSAutoRealm realm(state.cx, state.page);
  JS::RootedObject objectPrototype(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!objectPrototype) return false;
  for (Kind kind : {Kind::Navigator, Kind::Location}) {
    const char* name = kind == Kind::Location ? "location" : "navigator";
    if (!state.Instance(kind, objectPrototype) ||
        !state.Accessor(globalScopePrototype, name, int32_t(kind), JSPROP_ENUMERATE, WorkerGlobalGet) ||
        !helpers.Accessor("WorkerGlobalScope", globalScopePrototype, name)) return false;
  }
  // DOMStringList is exposed to Workers too (IDBDatabase.objectStoreNames).
  if (!InstallStringList(state) || !state.InstallWorkerContext(globalScopePrototype) ||
      !state.InstallWorkerEncoding(globalScopePrototype)) return false;
  // navigator.storage.getDirectory() is part of the Worker surface; the same
  // single StorageManager/OPFS installer that the main realm runs installs it.
  if (!InstallStorageManager(state) || !InstallOpfs(state)) return false;
  state.installed = true;
  return true;
}
}
