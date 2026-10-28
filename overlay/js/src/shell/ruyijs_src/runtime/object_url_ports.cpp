#include "runtime/object_url_ports.h"
#include "resource_network/cache_session.h"
#include "resource_network/worker_fetch_session.h"
#include "runtime/browsing_context.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/Realm.h"

namespace ruyijs {
namespace {
bool Origin(void* context, std::string* result) {
  auto& target = *static_cast<BrowsingContext*>(context);
  JSAutoRealm realm(target.cx, target.page);
  JS::RootedValue value(target.cx);
  if (!target.environment->Origin(&value) || !value.isString()) return false;
  JS::RootedString text(target.cx, value.toString());
  return ToUTF8(target.cx, text, result);
}
// One Worker's service session: its object URL frames go to the Web Platform
// owner's session, its network frames to this context's Resource Network
// owner. WebWorkers transports both without reading them.
struct WorkerSession {
  std::shared_ptr<web_platform::ObjectUrlSession> objectUrls;
  BrowsingContext* target;
  std::shared_ptr<resource_network::CacheSession> caches;
  std::shared_ptr<resource_network::WorkerFetchSession> fetches;
};
bool Serve(void* owner, std::string_view input, std::string* output) {
  auto& session = *static_cast<WorkerSession*>(owner);
  if (input.substr(0,2)=="K1") return session.caches && session.caches->Request(input,output);
  if (input.size()>1 && input[0]=='F') return session.fetches && session.fetches->Request(input,output);
  if (resource_network::Realm::IsWorkerNetworkFrame(input)) {
    auto* network = session.target->workerNetwork;
    if (!network) {
      JS_ReportErrorASCII(session.target->cx, "Worker network owner is not installed");
      return false;
    }
    return network->WorkerNetworkRequest(input, output);
  }
  return session.objectUrls->Request(input, output);
}
}
void BindObjectUrlCreation(BrowsingContext& target) {
  target.platform->SetObjectUrlPorts(Origin, &target, web_media::IsObjectUrlSource);
}
void BindObjectUrlWorkers(BrowsingContext& target) {
  target.workers->SetServiceFactory([](void* context, web_workers::Service* result) {
    auto& source = *static_cast<BrowsingContext*>(context);
    auto objectUrls = source.platform->OpenObjectUrlSession();
    if (!objectUrls) return false;
    auto* session = new (std::nothrow) WorkerSession{std::move(objectUrls), &source};
    if (!session) { JS_ReportOutOfMemory(source.cx); return false; }
    if (source.workerNetwork) {
      session->caches=source.workerNetwork->OpenCacheSession();
      session->fetches=source.workerNetwork->OpenWorkerFetchSession();
      if (!session->caches || !session->fetches) {delete session;return false;}
    }
    result->owner = std::shared_ptr<WorkerSession>(session);
    result->request = Serve;
    return true;
  }, &target);
}
void BindObjectUrlNetwork(BrowsingContext& target, resource_network::Realm& resources) {
  resources.Resources().SetObjectUrlPort([](void* context, const std::string& url,
                                           std::string* bytes, std::string* type) {
    return static_cast<web_platform::Realm*>(context)->ReadObjectUrl(url, bytes, type);
  }, target.platform.get());
  resources.SetCacheOriginPort(Origin,&target);
  target.workerNetwork = &resources;
}
}
