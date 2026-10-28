#include "runtime/window_message_ports.h"
#include "runtime/browsing_context.h"
#include "execution_scheduling/realm.h"

namespace ruyijs {
void BindWindowMessages(BrowsingContext& target, execution_scheduling::Realm& scheduling) {
  target.interaction->SetMicrotaskCheckpointPort([](void* context) {
    static_cast<execution_scheduling::Realm*>(context)->PerformMicrotaskCheckpoint();
  }, &scheduling);
  target.environment->SetPostTaskPort([](void* context, JS::HandleObject task) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(task);
  }, &scheduling);
  target.environment->SetPostMessageTaskPort([](void* context, JS::HandleObject task,
      JS::HandleObject source, JS::HandleObject target, uint64_t sequence, uint32_t kind) {
    return static_cast<execution_scheduling::Realm*>(context)->PostMessageTask(task, source, target, sequence, kind);
  }, &scheduling);
  // Window scrolling (docs/owners/BROWSER_ENVIRONMENT.md, 2026-10-22): the
  // scroll owner's notification task fires scroll/scrollend at this context's
  // document through the Interaction owner.
  target.environment->SetScrollEventPort([](void* context, const char* type) {
    auto& target = *static_cast<BrowsingContext*>(context);
    JS::RootedObject document(target.cx, target.document ? target.document->document() : nullptr);
    return !document || target.interaction->FireScroll(document, type);
  }, &target);
  target.environment->SetMessagePort([](void* context, JS::HandleObject window,
      JS::HandleValue data, const char* origin, JS::HandleObject source, JS::HandleObject ports) {
    return static_cast<interaction::Realm*>(context)->FireMessage(window, data, origin, source, ports);
  }, target.interaction.get());
  if (target.messaging) target.messaging->SetTaskPort([](void* context, JS::HandleObject task) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(task);
  }, &scheduling);
  // CSP violation events (docs/owners/CONTENT_SECURITY.md) are tasks too.
  if (target.security) target.security->SetTaskPort([](void* context, JS::HandleObject task) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(task);
  }, &scheduling);
  if (target.permissions) target.permissions->SetTaskPort([](void* context, JS::HandleObject task) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(task);
  }, &scheduling);
  // Geolocation success/error callbacks are tasks too (docs/owners/GEOLOCATION.md).
  if (target.geolocation) target.geolocation->SetTaskPort([](void* context, JS::HandleObject task) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(task);
  }, &scheduling);
  // Service Worker registry events (updatefound, statechange, message flush) are tasks.
  if (target.serviceWorker) target.serviceWorker->SetTaskPort([](void* context, JS::HandleObject task) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(task);
  }, &scheduling);
  // Speech Synthesis steps (start/boundary/end) are tasks too.
  if (target.speech) target.speech->SetTaskPort([](void* context, JS::HandleObject task, double delay_ms) {
    return static_cast<execution_scheduling::Realm*>(context)->PostDelayedTask(task, delay_ms);
  }, &scheduling);
  // ICE gathering steps of the WebRTC owner are delayed tasks too.
  if (target.rtc) target.rtc->SetTaskPort([](void* context, JS::HandleObject task, double delay_ms) {
    return static_cast<execution_scheduling::Realm*>(context)->PostDelayedTask(task, delay_ms);
  }, &scheduling);
  // Web Locks grants run their callback as a task.
  if (target.locks) target.locks->SetTaskPort([](void* context, JS::HandleObject task) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(task);
  }, &scheduling);
  if (target.wakeLock) target.wakeLock->SetTaskPort([](void* context, JS::HandleObject task) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(task);
  }, &scheduling);
  // Web Animations: timeline time from the one clock; pending play/pause and the
  // end timer are tasks on the same queue (docs/owners/WEB_ANIMATION.md).
  if (target.animation) {
    web_animation::Realm::SchedulingPorts ports;
    ports.now = [](void* context, JS::HandleObject page) {
      return static_cast<execution_scheduling::Realm*>(context)->DocumentNow(page);
    };
    ports.post = [](void* context, JS::HandleObject task) {
      return static_cast<execution_scheduling::Realm*>(context)->PostTask(task);
    };
    ports.postDelayed = [](void* context, JS::HandleObject task, double ms) {
      return static_cast<execution_scheduling::Realm*>(context)->PostDelayedTask(task, ms);
    };
    ports.context = &scheduling;
    target.animation->SetSchedulingPorts(ports);
  }
  // BroadcastChannel deliveries share the same macrotask queue.
  if (target.broadcast) target.broadcast->SetTaskPort([](void* context, JS::HandleObject task) {
    return static_cast<execution_scheduling::Realm*>(context)->PostTask(task);
  }, &scheduling);
}
}
