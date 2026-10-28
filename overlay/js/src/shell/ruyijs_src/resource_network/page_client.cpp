// The page's fetch entry. fetch(url, init) and fetch(request) both normalize
// through RequestInput, so one resource description feeds one PerformHttp call
// and one NetLog record per hop; the promise settles from a job of the engine job
// queue and an aborted signal rejects it before any request is made. The
// Response object itself is owned by response_object.cpp.
#include "resource_network/state.h"
#include "resource_network/request.h"
#include "resource_network/abort.h"
#include "resource_network/body.h"
#include "resource_network/websocket.h"
#include "resource_network/page_http.h"
#include "resource_network/native_fetch.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"
#include "js/Promise.h"
#include <cstring>

namespace ruyijs::resource_network {
namespace {
// The action carries result/method/url/headers/body/signal/redirect/prepared/credentials/source;
// the body is the draft's raw bytes as an ArrayBuffer.
bool FetchAction(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionOwner(args);
  JS::RootedObject info(cx, &js::GetFunctionNativeReserved(&args.callee(), 1).toObject());
  JS::RootedValue value(cx);
  JS::RootedObject result(cx);
  std::string method, url;
  HeaderPairs headers;
  if (!JS_GetElement(cx, info, 0, &value) || !value.isObject()) return false;
  result = &value.toObject();
  if (!JS_GetElement(cx, info, 1, &value) || !Convert(cx, value, &method)) return false;
  if (!JS_GetElement(cx, info, 2, &value) || !Convert(cx, value, &url)) return false;
  if (!JS_GetElement(cx, info, 3, &value) || !HeaderPairsArgument(cx, value, &headers)) return false;
  BodyRecord body;
  if (!JS_GetElement(cx, info, 4, &value) || !BodyInput(state, value, &body)) return false;
  // An already aborted signal rejects without recording or performing anything.
  if (!JS_GetElement(cx, info, 5, &value)) return false;
  if (AbortSignalled(value)) {
    JS::RootedValue reason(cx);
    return AbortReason(state, value, &reason) && JS::RejectPromise(cx, result, reason);
  }
  if (!JS_GetElement(cx, info, 7, &value)) return false;
  if (value.isObject()) return JS::ResolvePromise(cx, result, value);
  if (value.isString()) {
    std::string error;
    return Convert(cx, value, &error) && RejectWithTypeError(cx, result, error.c_str());
  }
  if (!JS_GetElement(cx, info, 6, &value)) return false;
  const auto redirect = RedirectMode(value.toInt32());
  HttpOptions options;options.initiator="fetch";
  if(!JS_GetElement(cx,info,10,&value))return false;options.cors=value.isTrue();
  if(!JS_GetElement(cx,info,8,&value))return false;
  options.credentials=Credentials(value.toInt32());
  if(!JS_GetElement(cx,info,9,&value)||!Convert(cx,value,&options.source))return false;
  if(!JS_GetElement(cx,info,11,&value)||!Convert(cx,value,&options.referrer)||
     !JS_GetElement(cx,info,12,&value)||!Convert(cx,value,&options.referrerPolicy)||
     !JS_GetElement(cx,info,13,&value)||!Convert(cx,value,&options.mode))return false;
  HttpResult http;
  if (!PerformHttp(state.resources, method, url, headers, body.bytes, &http, redirect,options))
    return RejectWithTypeError(cx, result, http.error.empty() ? "fetch failed" : http.error.c_str());
  JS::RootedObject response(cx);
  JS::RootedValue resolved(cx);
  if (!ResponseObject(state, http, url, method, options.mode,
                      options.credentials == Credentials::Include, &response)) return false;
  resolved.setObject(*response);
  return JS::ResolvePromise(cx, result, resolved);
}

// The request body crosses the job boundary as bytes, so a binary body is not
// reinterpreted as text on the way.
bool BodyValue(State& state, const std::string& body, JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  JS::RootedObject buffer(cx, JS::NewArrayBuffer(cx, body.size()));
  if (!buffer) return false;
  size_t length = 0;
  bool shared = false;
  uint8_t* data = nullptr;
  JS::GetArrayBufferLengthAndData(buffer, &length, &shared, &data);
  if (!body.empty()) std::memcpy(data, body.data(), body.size());
  result.setObject(*buffer);
  return true;
}

// The promise of a fetch() that runs on the asynchronous transport: the JS
// thread keeps running while the request is in flight, as in Firefox, and the
// promise settles from the response task.
bool SettleFetch(JSContext* cx, JS::HandleObject target, NativeFetchOutcome outcome,
                 JS::HandleObject response, JS::HandleValue reason) {
  JS::RootedObject promise(cx, target);
  JSAutoRealm realm(cx, promise);
  switch (outcome) {
    case NativeFetchOutcome::Complete: {
      JS::RootedValue value(cx, JS::ObjectValue(*response));
      return JS_WrapValue(cx, &value) && JS::ResolvePromise(cx, promise, value);
    }
    case NativeFetchOutcome::Aborted:
    case NativeFetchOutcome::NativeFailure: {
      JS::RootedValue value(cx, reason);
      return JS_WrapValue(cx, &value) && JS::RejectPromise(cx, promise, value);
    }
    case NativeFetchOutcome::NetworkFailure:
      return RejectWithTypeError(cx, promise, "NetworkError when attempting to fetch resource.");
    case NativeFetchOutcome::Retired: return true;
  }
  return true;
}
const NativeFetchSteps kSettleFetch = {SettleFetch};
bool Fetch(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionOwner(args);
  state.Observe(state.workerScope ? "WorkerGlobalScope" : "Window", "fetch", int(ApiAccess::Call));
  if (!argc) return TypeErrorPromise(cx, "fetch requires a URL", args.rval());
  JS::RootedValue init(cx);
  if (argc > 1) init.set(args[1]);
  RequestDraft draft;
  JS::RootedValue signal(cx);
  if (!RequestInput(state, args[0], init, &draft, &signal)) {
    JS::RootedValue error(cx);
    if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx, &error)) return false;
    JS_ClearPendingException(cx);
    return Rejected(cx, error, args.rval());
  }
  if (draft.url.compare(0, 5, "blob:") != 0) {
    JS::RootedObject pending(cx, JS::NewPromiseObject(cx, nullptr));
    if (!pending || !FetchNative(state, draft, signal, pending, kSettleFetch)) return false;
    args.rval().setObject(*pending);
    return true;
  }
  JS::RootedObject action(cx, NativeFunction(state, FetchAction, "fetch action", 0));
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  JS::RootedObject trigger(cx, JS::NewPromiseObject(cx, nullptr));
  JS::RootedObject info(cx, JS::NewArrayObject(cx, 14));
  JS::RootedObject list(cx, JS::NewArrayObject(cx, draft.headers.size()));
  if (!action || !promise || !trigger || !info || !list) return false;
  for (size_t index = 0; index < draft.headers.size(); ++index) {
    JS::RootedObject pair(cx, JS::NewArrayObject(cx, 2));
    JS::RootedString name(cx, JS_NewStringCopyZ(cx, draft.headers[index].first.c_str()));
    JS::RootedString field(cx, JS_NewStringCopyZ(cx, draft.headers[index].second.c_str()));
    if (!pair || !name || !field || !JS_SetElement(cx, pair, 0, name) ||
        !JS_SetElement(cx, pair, 1, field) || !JS_SetElement(cx, list, uint32_t(index), pair))
      return false;
  }
  JS::RootedValue method_value(cx), url_value(cx), body_value(cx), headers_value(cx);
  JS::RootedValue promise_value(cx, JS::ObjectValue(*promise));
  const auto redirect = draft.redirect == "manual" ? RedirectMode::Manual :
                        draft.redirect == "error" ? RedirectMode::Error : RedirectMode::Follow;
  JS::RootedValue redirect_value(cx, JS::Int32Value(int(redirect)));
  if (!Value(cx, draft.method, &method_value) || !Value(cx, draft.url, &url_value) ||
      !BodyValue(state, draft.body, &body_value)) return false;
  // Capture a Blob URL at initiation. Revocation releases the catalog entry,
  // while this traced Response remains owned by the existing promise job.
  JS::RootedValue prepared(cx);
  if (draft.url.compare(0, 5, "blob:") == 0 && !AbortSignalled(signal)) {
    HttpResult http;
    if (PerformHttp(state.resources, draft.method, draft.url, draft.headers, draft.body, &http, redirect)) {
      JS::RootedObject response(cx);
      if (!ResponseObject(state, http, draft.url, draft.method, draft.mode,
                          draft.credentials == "include", &response)) return false;
      prepared.setObject(*response);
    } else if (!Value(cx, http.error, &prepared)) return false;
  }
  headers_value.setObject(*list);
  HttpContext context;
  if(!state.resources.ReadHttpContext(&context))return false;
  JS::RootedValue source_value(cx);
  if(!Value(cx,context.source,&source_value))return false;
  JS::RootedValue referrer(cx),policy(cx),mode(cx);
  if(!Value(cx,draft.referrer,&referrer)||!Value(cx,draft.referrer_policy,&policy)||!Value(cx,draft.mode,&mode))return false;
  JS::RootedValue credentials(cx,JS::Int32Value(int(draft.credentials=="omit"?Credentials::Omit:
      draft.credentials=="include"?Credentials::Include:Credentials::SameOrigin)));
  if (!JS_SetElement(cx, info, 0, promise_value) || !JS_SetElement(cx, info, 1, method_value) ||
      !JS_SetElement(cx, info, 2, url_value) || !JS_SetElement(cx, info, 3, headers_value) ||
      !JS_SetElement(cx, info, 4, body_value) || !JS_SetElement(cx, info, 5, signal) ||
      !JS_SetElement(cx, info, 6, redirect_value) || !JS_SetElement(cx, info, 7, prepared) ||
      !JS_SetElement(cx,info,8,credentials)||!JS_SetElement(cx,info,9,source_value)||
      !JS_SetElement(cx,info,10,draft.mode=="cors")||
      !JS_SetElement(cx,info,11,referrer)||!JS_SetElement(cx,info,12,policy)||
      !JS_SetElement(cx,info,13,mode))
    return false;
  js::SetFunctionNativeReserved(action, 1, JS::ObjectValue(*info));
  JS::RootedObject handler(cx, action);
  if (!JS::AddPromiseReactions(cx, trigger, handler, handler) ||
      !JS::ResolvePromise(cx, trigger, JS::UndefinedHandleValue)) return false;
  args.rval().setObject(*promise);
  return true;
}
}  // namespace

// The one fetch factory: the Window global's own property, or a Worker's
// WorkerGlobalScope.prototype member (WindowOrWorkerGlobalScope).
bool InstallFetch(State& s, PageHelpers& helpers, JS::HandleObject target, const char* iface) {
  JSContext* cx = s.cx;
  JS::RootedObject fetch(cx, NativeFunction(s, Fetch, "fetch", 1));
  if (!fetch || !JS_DefineProperty(cx, target, "fetch", fetch, JSPROP_ENUMERATE)) return false;
  return helpers.Member(fetch, iface, "fetch", "call");
}
bool InstallPage(State& s, PageHelpers& helpers) {
  return InstallFetch(s, helpers, s.page, "Window") && InstallXhr(s, helpers) &&
         InstallStorage(s, helpers) && InstallWebSocket(s, helpers) && InstallEventSource(s);
}
}  // namespace ruyijs::resource_network
