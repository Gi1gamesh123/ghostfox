#include "runtime/top_level_assembly.h"
#include "runtime/api_monitor.h"
#include "runtime/browsing_context.h"
#include "runtime/page_load.h"
#include "execution_scheduling/realm.h"

namespace ruyijs {
namespace {
void BindMonitor(BrowsingContext& target, ApiMonitor* monitor) {
  void (*port)(void*, const char*, const char*, int) = monitor ? &ApiMonitor::Port : nullptr;
  target.document->SetApiMonitor(port, monitor);
  target.interaction->SetApiMonitor(port, monitor);
  target.environment->SetApiMonitor(port, monitor);
  if (target.resources) target.resources->SetApiMonitor(port, monitor);
}
}
void BindDocumentEvents(BrowsingContext& target, PageLoad& pages) {
  target.environment->SetPopStatePort([](void* context, JS::HandleObject window,
                                         JS::HandleValue state) {
    return static_cast<interaction::Realm*>(context)->FirePopState(window, state);
  }, target.interaction.get());
  // window.reportError reports the value and fires one trusted error event on
  // the window through the Interaction owner's dispatch tree.
  target.environment->SetEventPort([](void* context, JS::HandleObject window, const char* type) {
    return static_cast<interaction::Realm*>(context)->Fire(window, type, false, true);
  }, target.interaction.get());
  target.environment->SetNavigatePort([](void* context, const char* url, bool replace, bool reload) {
    return static_cast<PageLoad*>(context)->RequestNavigation(url ? url : "", replace, reload);
  }, &pages);
  target.environment->SetHashChangePort([](void* context, JS::HandleObject window,
                                           const char* oldURL, const char* newURL) {
    return static_cast<interaction::Realm*>(context)->FireHashChange(window, oldURL, newURL);
  }, target.interaction.get());
}
bool BindTopLevelDocument(void* assembly, BrowsingContext& target) {
  auto& owners = *static_cast<TopLevelAssembly*>(assembly);
  BindDocumentEvents(target, owners.pages);
  // The shared IndexedDB store's task moves to this document; the port to the
  // one Scheduling queue that runtime.cpp bound stays with the store.
  if (!target.indexeddb->AdoptStore()) return false;
  // Web Audio reads currentTime from the Context's one clock owner.
  target.audio->SetClockPort([](void* context) {
    return static_cast<execution_scheduling::Realm*>(context)->ClockSeconds();
  }, &owners.scheduling);
  target.audio->SetTaskPort([](void* context, JS::HandleObject callback) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(callback);
  }, &owners.scheduling);
  if (owners.monitor.enabled()) BindMonitor(target, &owners.monitor);
  return true;
}
void RetireTopLevelDocument(void* assembly, BrowsingContext& target) {
  auto& owners = *static_cast<TopLevelAssembly*>(assembly);
  target.interaction->RetireCloseWatchers();
  JS::RootedObject global(target.cx, target.page);
  owners.scheduling.Retire(global);
  if (target.navigatorNetwork) target.navigatorNetwork->RetireBodyConsumers();
  if (target.navigatorNetwork) target.navigatorNetwork->RetireNativeFetches();
  if (target.navigatorNetwork) target.navigatorNetwork->RetireWebTransports();
  owners.frames.RetireUnder(target);
  target.workers->Stop();
  target.document->SetScriptPort(nullptr, nullptr);
  target.document->SetFormPorts(nullptr, nullptr, nullptr);
  target.document->RetireImageLoads();
  target.platform->ClearDocumentUrls();
  target.environment->RetireMessages();
  target.messaging->Retire();
  target.environment->SetNavigatePort(nullptr, nullptr);
  target.environment->SetHashChangePort(nullptr, nullptr);
  target.environment->SetPopStatePort(nullptr, nullptr);
  target.environment->SetEventPort(nullptr, nullptr);
  target.audio->SetClockPort(nullptr, nullptr);
  target.audio->SetTaskPort(nullptr, nullptr);
  BindMonitor(target, nullptr);
  // A retired document's timelines go inactive and queue no further animation work.
  if (target.animation) target.animation->SetSchedulingPorts({});
}
}
