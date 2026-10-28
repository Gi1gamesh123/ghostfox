// Worker assembly of the IndexedDB, WebGPU, Permissions, Web Locks, WebRTC and
// XHR/WebSocket owners (worker_owners.h). Every port bound here is cleared in
// the destructor before the owners die.
#include "runtime/worker_owners.h"
#include "browser_environment/environment.h"
#include "execution_scheduling/realm.h"
#include "interaction/realm.h"
#include "resource_network/realm.h"
#include "web_gpu/realm.h"
#include "web_indexeddb/realm.h"
#include "web_locks/realm.h"
#include "web_permissions/realm.h"
#include "web_workers/child.h"
#include "web_rtc/realm.h"
#include "document_model/exception.h"
#include "jsapi.h"

namespace ruyijs {
namespace {
bool PostTask(void* context, JS::HandleObject task) {
  return static_cast<execution_scheduling::Realm*>(context)->PostTask(task);
}
}  // namespace

struct WorkerOwners::Parts {
  JSContext* cx;
  PageHelpers& helpers;
  browser_environment::Environment& environment;
  resource_network::Realm& resources;
  execution_scheduling::Realm& scheduling;
  web_workers::Child& child;
  void (*checkpoint)(JSContext*);
  web_indexeddb::Realm indexeddb;
  web_gpu::Realm gpu;
  web_permissions::Realm permissions;
  web_locks::Realm locks;
  web_rtc::Realm rtc;
  Parts(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events,
        browser_environment::Environment& environment, resource_network::Realm& resources,
        execution_scheduling::Realm& scheduling, web_workers::Child& child, void (*checkpoint)(JSContext*))
      : cx(cx), helpers(helpers), environment(environment), resources(resources), scheduling(scheduling),
        child(child), checkpoint(checkpoint),
        indexeddb(cx, page, helpers, events, environment, nullptr),
        gpu(cx, page, helpers, events, environment),
        permissions(cx, page, helpers, events, nullptr),
        locks(cx, page, helpers, environment, nullptr),
        rtc(cx, page, helpers, events) {}
};

WorkerOwners::WorkerOwners(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
                           interaction::Realm& events, browser_environment::Environment& environment,
                           resource_network::Realm& resources, execution_scheduling::Realm& scheduling,
                           web_workers::Child& child, void (*checkpoint)(JSContext*))
    : parts_(std::make_unique<Parts>(cx, page, helpers, events, environment, resources, scheduling, child,
                                     checkpoint)) {}

WorkerOwners::~WorkerOwners() {
  Parts& p = *parts_;
  p.environment.SetPermissionsPort(nullptr, nullptr);
  p.environment.SetPermissionsDescribe(nullptr, nullptr);
  p.permissions.SetTaskPort(nullptr, nullptr);
  p.locks.SetTaskPort(nullptr, nullptr);
  p.indexeddb.SetTaskPort(nullptr, nullptr);
  p.resources.SetXhrTaskPort(nullptr, nullptr, nullptr);
  p.child.SetScriptPort(nullptr, nullptr);
}

bool WorkerOwners::Install(JS::HandleObject globalScopePrototype) {
  Parts& p = *parts_;
  // Network transports: XHR delivery and IndexedDB jobs use the Worker's one queue.
  p.resources.SetXhrTaskPort(&PostTask, &p.scheduling, p.checkpoint);
  p.indexeddb.SetTaskPort(&PostTask, &p.scheduling);
  p.permissions.SetTaskPort(&PostTask, &p.scheduling);
  p.locks.SetTaskPort(&PostTask, &p.scheduling);
  p.child.SetScriptPort([](void* context, std::string_view base, std::string_view input, std::string* url,
                           std::string* source, bool* ok) {
    return static_cast<resource_network::Realm*>(context)->FetchWorkerScript(base, input, url, source, ok);
  }, &p.resources);
  if (!p.resources.InstallWorkerTransports(p.helpers, !p.child.service()) || !p.indexeddb.InstallWorker(globalScopePrototype))
    return false;
  // WorkerNavigator members in Firefox order: permissions, gpu, then locks.
  JS::RootedObject navigatorPrototype(p.cx, p.environment.navigatorPrototype());
  if (!navigatorPrototype || !p.permissions.Install(navigatorPrototype) || !p.gpu.Install() || !p.locks.Install())
    return false;
  p.environment.SetPermissionsPort([](void* context, JS::HandleObject group) {
    return static_cast<web_permissions::Realm*>(context)->Configure(group);
  }, &p.permissions);
  p.environment.SetPermissionsDescribe([](void* context, JS::MutableHandleValue result) {
    return static_cast<web_permissions::Realm*>(context)->Describe(result);
  }, &p.permissions);
  // The DedicatedWorker members of the RTC owner; RTCError is the DOMException
  // owner's native storage, as in a Window.
  web_rtc::Realm::ExceptionPorts rtcExceptions;
  rtcExceptions.create = &document_model::CreateDerivedException;
  rtcExceptions.data = &document_model::DerivedExceptionData;
  p.rtc.SetExceptionPorts(rtcExceptions);
  // RTC worker interfaces exist in dedicated Workers only (Firefox 158 service
  // and shared worker inventories).
  return p.gpu.ConfigureExposure() && p.locks.ConfigureExposure() &&
         (!p.child.dedicated() || p.rtc.InstallWorker());
}

bool WorkerOwners::Finish(JS::HandleObject globalScopePrototype) {
  Parts& p = *parts_;
  return p.scheduling.PlaceWorkerMembers(globalScopePrototype) && p.child.OrderGlobalScope() &&
         p.environment.OrderNavigator();
}
}  // namespace ruyijs
