#include "resource_network/state.h"
#include "resource_network/async_http.h"
#include "web_platform/request_body.h"
#include "domain/browser_environment/url_value.h"
#include "jsapi.h"
#include "js/Object.h"
#include "jsfriendapi.h"
#include <new>

namespace ruyijs::resource_network {
namespace {
using UrlValue = ruyijs::domain::browser_environment::UrlValue;
constexpr size_t kPendingLimit = 64, kPendingBytes = 8 * 1024 * 1024;
struct BeaconRequest {
  std::string url, bytes, source, site;
  HeaderPairs headers;
  std::shared_ptr<BeaconBudget> budget;
  ~BeaconRequest() {
    if (budget) { --budget->count; budget->bytes -= bytes.size(); }
  }
};
void Finalize(JS::GCContext*, JSObject* object) {
  auto value = JS::GetReservedSlot(object, 0);
  if (!value.isUndefined()) delete static_cast<BeaconRequest*>(value.toPrivate());
}
const JSClassOps kTaskOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, Finalize};
const JSClass kTaskClass = {"BeaconRequest", JSCLASS_HAS_RESERVED_SLOTS(1) |
    JSCLASS_FOREGROUND_FINALIZE, &kTaskOps};
bool Deliver(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& owner = *FunctionOwner(args);
  JS::RootedObject packet(cx, &js::GetFunctionNativeReserved(&args.callee(), 1).toObject());
  auto value = JS::GetReservedSlot(packet, 0);
  args.rval().setUndefined();
  if (value.isUndefined()) return true;
  std::shared_ptr<BeaconRequest> request(static_cast<BeaconRequest*>(value.toPrivate()));
  JS::SetReservedSlot(packet, 0, JS::UndefinedValue());
  HttpOptions options;options.initiator="beacon";
  options.source=request->source;options.site=request->site;
  // The page observes admission, not a response: the request runs on the
  // asynchronous transport and the JS thread does not wait for it. The pending
  // budget is released when the response (or failure) arrives.
  const std::string method = "POST";
  AsyncHttpTicket ticket;
  if (owner.asyncSource &&
      StartAsyncHttp(owner.asyncSource, {method, request->url, request->headers, request->bytes,
                                         RedirectMode::Follow, options},
                     {request, [](void*, AsyncHttpResult&&) noexcept {}}, &ticket).code == ErrorCode::None)
    return true;
  HttpResult ignored;  // no asynchronous source (detached realm): the original synchronous path
  (void)PerformHttp(owner.resources, method, request->url, request->headers,
                    request->bytes, &ignored,RedirectMode::Follow,options);
  return true;
}
}
bool Realm::Beacon(JSContext* cx, const std::string& base, const std::string& documentURL,
                   const std::string& input, web_platform::RequestBody&& snapshot, bool* accepted) {
  *accepted = false;
  if (!state_) return false;
  State& state = *state_;
  UrlValue url;
  if (!UrlValue::Resolve(input, base, &url) ||
      (url.protocol() != "http:" && url.protocol() != "https:"))
    return Error(cx, "Navigator.sendBeacon requires a valid HTTP(S) URL");
  if (snapshot.tooLarge || url.href().size() > Limits::key_bytes || !state.xhrTask ||
      state.beaconBudget->count >= kPendingLimit ||
      snapshot.bytes.size() > kPendingBytes - state.beaconBudget->bytes) return true;
  std::unique_ptr<BeaconRequest> request(new (std::nothrow) BeaconRequest);
  if (!request) { JS_ReportOutOfMemory(cx); return false; }
  url.set_hash("");
  request->url = url.href();
  HttpContext context;
  if(!state.resources.ReadHttpContext(&context))return false;
  request->source=documentURL;request->site=context.site;
  request->bytes = std::move(snapshot.bytes);
  if (!snapshot.contentType.empty())
    request->headers.emplace_back("Content-Type", std::move(snapshot.contentType));
  UrlValue source(documentURL);
  request->headers.emplace_back("Origin", source.origin());
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject packet(cx, JS_NewObjectWithGivenProto(cx, &kTaskClass, nullptr));
  JS::RootedObject task(cx, NativeFunction(state, Deliver, "beacon delivery", 0));
  if (!packet || !task) return false;
  request->budget = state.beaconBudget;
  ++request->budget->count;
  request->budget->bytes += request->bytes.size();
  JS::SetReservedSlot(packet, 0, JS::PrivateValue(request.release()));
  js::SetFunctionNativeReserved(task, 1, JS::ObjectValue(*packet));
  const bool posted = state.keepaliveTask ? state.keepaliveTask(state.keepaliveTaskContext, task)
                                          : state.xhrTask(state.xhrTaskContext, task);
  if (!posted) {
    auto* rejected = static_cast<BeaconRequest*>(JS::GetReservedSlot(packet, 0).toPrivate());
    JS::SetReservedSlot(packet, 0, JS::UndefinedValue());
    delete rejected;
    return false;
  }
  *accepted = true;
  return true;
}
}
