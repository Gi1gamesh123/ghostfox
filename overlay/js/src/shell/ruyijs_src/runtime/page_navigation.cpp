// Top-level cross-document navigation (docs/owners/TOP_LEVEL.md), in the order
// Firefox 157 shows (tests/fixtures/top_level_navigation_firefox.json):
// beforeunload when the navigation starts and the calling script continues;
// the request is a task; its response commits as a later task, so the current
// document keeps running meanwhile; the commit fires pagehide and unload at the
// current document and produces the new document in a new realm. Application
// orchestration only: Browser Environment commits the URL, the resource owner
// performs the request over Necko, Interaction fires the events and TopLevel
// builds and retires the realms.
#include "runtime/secure_exposure.h"
#include "runtime/page_load.h"
#include "runtime/browsing_context.h"
#include "runtime/exception_capture.h"
#include "runtime/top_level.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "execution_scheduling/realm.h"
#include "execution_scheduling/navigation_timing.h"
#include "web_crypto/realm.h"
#include "web_geolocation/realm.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallArgs.h"
#include "js/CharacterEncoding.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/String.h"
#include <cctype>

namespace ruyijs {
namespace {
enum Slot { Loader, Ticket, Target, Replace, Browser, Count };
const JSClass kNavigationTask = {"Navigation task", JSCLASS_HAS_RESERVED_SLOTS(Count)};
// The URL a navigation commits: the request URL, or after redirects the final
// response URL, which inherits the request's fragment when it has none.
std::string Committed(const std::string& url, const resource_network::HttpResult& http) {
  if (!http.redirected || http.finalURL.empty()) return url;
  const size_t hash = url.find('#');
  if (hash == std::string::npos || http.finalURL.find('#') != std::string::npos)
    return http.finalURL;
  return http.finalURL + url.substr(hash);
}
// The Context's one clock as an epoch millisecond timestamp, the unit
// performance.timeOrigin and the legacy timing fields report.
double Epoch(execution_scheduling::Realm& scheduling) {
  const auto snapshot = scheduling.Inspect();
  return snapshot.epoch + snapshot.now;
}
// One task created in the current realm, so it is dropped with the document
// that queued it if that document is retired first.
bool Post(JSContext* cx, execution_scheduling::Realm& scheduling, JSNative deliver,
          JS::HandleObject task) {
  JS::RootedFunction fn(cx, js::NewFunctionWithReserved(cx, deliver, 0, 0, "Navigation task"));
  if (!fn) return false;
  JS::RootedObject callback(cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(callback, 0, JS::ObjectValue(*task));
  return scheduling.PostTask(callback);
}
}
bool PageLoad::BeforeUnload() {
  BrowsingContext& active = top_.Active();
  JS::RootedObject window(cx_, active.page);
  return active.interaction->FireBeforeUnload(window);
}
bool PageLoad::BrowserNavigation(const std::string& url) {
  // The host `page` "navigate" action commits on this call, the same way the
  // host `load` does: a browser-started top-level navigation has no page script
  // underneath it that must keep running, so it need not be queued as a task.
  // Running it inline also keeps it working where posted engine tasks are not
  // delivered between host RPC calls, as in the standalone shell under Wine
  // (issue #1). In-page navigation (RequestNavigation, below) keeps Firefox's
  // async two-task model so the calling script continues first.
  const uint64_t ticket = ++ticket_;
  if (!BeforeUnload()) return false;
  // A beforeunload listener may have started its own navigation; it wins.
  if (Started(ticket)) return true;
  return Navigate(url, false, "GET", "", "", true, true);
}
bool PageLoad::RequestNavigation(const std::string& url, bool replace, bool reload, bool browser) {
  if (reload && top_.Active().browserInitiated) browser = true;
  const uint64_t ticket = ++ticket_;
  if (!BeforeUnload()) return false;
  JS::RootedObject task(cx_, JS_NewObjectWithGivenProto(cx_, &kNavigationTask, nullptr));
  JS::RootedString target(cx_, JS_NewStringCopyUTF8N(cx_, JS::UTF8Chars(url.data(), url.size())));
  if (!task || !target) return false;
  JS::SetReservedSlot(task, Loader, JS::PrivateValue(this));
  JS::SetReservedSlot(task, Ticket, JS::DoubleValue(double(ticket)));
  JS::SetReservedSlot(task, Target, JS::StringValue(target));
  JS::SetReservedSlot(task, Replace, JS::BooleanValue(replace));
  JS::SetReservedSlot(task, Browser, JS::BooleanValue(browser));
  return Post(cx_, scheduling_, DeliverNavigation, task);
}
bool PageLoad::DeliverNavigation(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject task(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  args.rval().setUndefined();
  auto& loader = *static_cast<PageLoad*>(JS::GetReservedSlot(task, Loader).toPrivate());
  // A later navigation (or host load) replaced this one before it ran.
  if (loader.Started(uint64_t(JS::GetReservedSlot(task, Ticket).toNumber()))) return true;
  JS::RootedString target(cx, JS::GetReservedSlot(task, Target).toString());
  std::string url;
  return ToUTF8(cx, target, &url) &&
         loader.Navigate(url, JS::GetReservedSlot(task, Replace).toBoolean(), "GET", "", "",
                         JS::GetReservedSlot(task, Browser).toBoolean());
}
bool PageLoad::Navigate(const std::string& url, bool replace, const std::string& method,
                        const std::string& requestBody, const std::string& contentType,
                        bool browserInitiated, bool commitInline) {
  BrowsingContext& previous = top_.Active();
  JS::RootedObject before(cx_, previous.page);
  JSAutoRealm realm(cx_, before);
  auto pending = std::make_unique<Pending>();
  pending->ticket = ticket_;
  pending->url = url;
  pending->replace = replace;
  resource_network::Model& network = Network(previous);
  resource_network::HttpContext source;
  if (!network.ReadHttpContext(&source)) return false;
  // A target the transport cannot serve still navigates: the request is in
  // the one NetLog and the new document is empty.
  resource_network::HeaderPairs headers;
  if (!contentType.empty()) headers.emplace_back("Content-Type", contentType);
  resource_network::HttpOptions options;
  options.navigation = true;
  options.source = source.source;
  options.site = source.site;
  options.browserInitiated = options.userActivated = browserInitiated;
  pending->browserInitiated = browserInitiated;
  // The navigation's own timing, on the Context's clock: this document's
  // navigation start, then the request and response instants the transport
  // records. Firefox measures the same way (the navigation entry is completed
  // when the response ends, before commit).
  pending->origin = Epoch(scheduling_);
  pending->request_start = pending->origin;
  resource_network::PerformHttp(network, method, url, headers, requestBody, &pending->http,
                                resource_network::RedirectMode::Follow, options);
  pending->response_end = Epoch(scheduling_);
  pending->response_start = pending->response_end;
  pending->redirects = uint16_t(pending->http.redirected ? 1 : 0);
  pending_ = std::move(pending);
  // The host `navigate` action commits here, on the calling stack; an in-page
  // navigation posts the commit as a later task so the current document keeps
  // running meanwhile, as Firefox does.
  if (commitInline) return Commit(pending_->ticket);
  JS::RootedObject task(cx_, JS_NewObjectWithGivenProto(cx_, &kNavigationTask, nullptr));
  if (!task) return false;
  JS::SetReservedSlot(task, Loader, JS::PrivateValue(this));
  JS::SetReservedSlot(task, Ticket, JS::DoubleValue(double(pending_->ticket)));
  return Post(cx_, scheduling_, DeliverCommit, task);
}
bool PageLoad::DeliverCommit(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject task(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  args.rval().setUndefined();
  auto& loader = *static_cast<PageLoad*>(JS::GetReservedSlot(task, Loader).toPrivate());
  return loader.Commit(uint64_t(JS::GetReservedSlot(task, Ticket).toNumber()));
}
bool PageLoad::Commit(uint64_t ticket) {
  if (Started(ticket) || !pending_ || pending_->ticket != ticket) return true;
  std::unique_ptr<Pending> response = std::move(pending_);
  const auto& http = response->http;
  // Firefox stays on the current document for a No Content response.
  if (http.status == 204 || http.status == 205) return true;
  // Firefox renders the body of any other HTTP response, error statuses
  // included (a server's 404 page, a 412 challenge page); only a transport
  // failure (no status) commits an empty document.
  std::string body;
  if (http.status != 0) body = http.body;
  BrowsingContext& previous = top_.Active();
  {
    // Unload the current document at its Window, on its own dispatch tree.
    // Listener errors are reported by the event owner; a navigation a listener
    // starts is dropped with the document's tasks, as Firefox ignores it.
    JS::RootedObject window(cx_, previous.page);
    JSAutoRealm realm(cx_, window);
    if (!previous.interaction->FirePageTransition(window, "pagehide", false) ||
        !previous.interaction->Fire(window, "unload", false, false)) return false;
    checkpoint_(cx_);
    if (JS_IsExceptionPending(cx_)) CaptureOperationFailure(cx_);
  }
  // A listener above may have started another navigation; this one still
  // commits, and the newer ticket now belongs to a retired document's task.
  ticket_ = ticket;
  BrowsingContext* next = nullptr;
  if (!top_.Advance(&next)) return false;
  next->browserInitiated = response->browserInitiated;
  JS::RootedObject after(cx_, next->page);
  Owners owners = OwnersOf(*next, after);
  JSAutoRealm realm(cx_, after);
  const std::string committed = Committed(response->url, http);
  if (!owners.environment.CommitNavigation(committed, response->replace)) {
    if (committed == response->url) return false;
    JS_ClearPendingException(cx_);
    if (!owners.environment.CommitNavigation(response->url, response->replace)) return false;
  }
  // This document's navigation record, published before any of its scripts run
  // (docs/owners/NAVIGATION_TIMING.md): performance.timeOrigin,
  // performance.timing.* and the navigation entry all read one record.
  execution_scheduling::NavigationTiming timing;
  timing.present = true;
  timing.time_origin = response->origin;
  // Offsets from this document's own navigation start.
  timing.request_start = response->request_start - response->origin;
  timing.response_start = response->response_start - response->origin;
  timing.response_end = response->response_end - response->origin;
  timing.redirect_count = response->redirects;
  timing.status = uint16_t(http.status);
  timing.encoded_size = http.encodedBytes;
  timing.decoded_size = http.decodedBytes;
  timing.protocol = http.protocol;
  timing.name = committed;
  {
    const auto hash = timing.name.find('#');
    if (hash != std::string::npos) timing.name.resize(hash);
  }
  for (const auto& field : http.headers) {
    std::string key = field.first;
    for (auto& c : key) c = char(std::tolower(static_cast<unsigned char>(c)));
    if (key != "content-type") continue;
    timing.content_type = field.second.substr(0, field.second.find(';'));
    while (!timing.content_type.empty() && timing.content_type.back() == ' ') timing.content_type.pop_back();
  }
  if (!scheduling_.RecordNavigation(after, timing)) return false;
  owners.document.SetDocumentReferrer(http.referrer);
  // The secure-context surface follows the committed document URL.
  if (!ConfigureSecureExposure(*next)) return false;
  JS::RootedString markup(cx_, JS_NewStringCopyUTF8N(cx_, JS::UTF8Chars(body.data(), body.size())));
  if (!markup) return false;
  JS::RootedValue ignored(cx_);
  uint32_t executed = 0;
  JS::RootedObject completion(cx_);
  return Produce(owners, markup, &ignored, &executed, false, &completion);
}
}
