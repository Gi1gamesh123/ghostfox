// EventSource connection over this owner's native fetch. The transport delivers a
// response when it ends, so the stream is interpreted then (domain EventStreamParser)
// and the connection is re-established after the reconnection time, as Firefox does
// when a server closes the stream (docs/DEVIATIONS.md: no incremental delivery of a
// long-lived stream).
#include "resource_network/event_source.h"
#include "resource_network/body.h"
#include "resource_network/native_fetch.h"
#include "resource_network/request_value.h"
#include "domain/resource_network/event_stream.h"
#include "domain/browser_environment/url_value.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/String.h"
#include <cctype>

namespace ruyijs::resource_network {
namespace sse = ::ruyijs::domain::resource_network;
namespace {
int32_t ReadyState(JSObject* self) { return JS::GetReservedSlot(self, EState).toInt32(); }
bool Text(JSContext* cx, JS::HandleValue value, std::string* out) {
  JS::RootedString text(cx, value.toString());
  JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, text);
  if (!utf8) return false;
  *out = utf8.get();
  return true;
}
// UTF-8 decode (replacement for invalid sequences), as the stream decoder does.
bool Decode(JSContext* cx, std::string_view bytes, std::u16string* out) {
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(bytes.data(), bytes.size())));
  if (!text) return false;
  out->resize(JS_GetStringLength(text));
  return JS_CopyStringChars(cx, mozilla::Range<char16_t>(out->data(), out->size()), text);
}
bool Fire(State& state, JS::HandleObject self, const char* type) { return state.events->Fire(self, type, false, false); }
bool Reconnect(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject callee(cx, &args.callee());  // before rval(), which shares the callee slot
  args.rval().setUndefined();
  JS::RootedObject self(cx, &js::GetFunctionNativeReserved(callee, 0).toObject());
  if (JS::GetReservedSlot(self, EGeneration) != js::GetFunctionNativeReserved(callee, 1) || ReadyState(self) != 0)
    return true;  // closed (or re-opened) meanwhile
  return EventSourceConnect(*static_cast<State*>(JS::GetReservedSlot(self, EOwner).toPrivate()), self);
}
// "Reestablish the connection": CONNECTING, error, then a new request after the reconnection time.
bool Reestablish(State& state, JS::HandleObject self) {
  JSContext* cx = state.cx;
  JS::SetReservedSlot(self, EState, JS::Int32Value(0));
  if (!Fire(state, self, "error")) return false;
  if (ReadyState(self) != 0 || !state.abortDelay) return true;
  JSFunction* fn = js::NewFunctionWithReserved(cx, Reconnect, 0, 0, "EventSource reconnect");
  if (!fn) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(task, 0, JS::ObjectValue(*self));
  js::SetFunctionNativeReserved(task, 1, JS::GetReservedSlot(self, EGeneration));
  return state.abortDelay(state.abortDelayContext, task, JS::GetReservedSlot(self, ERetry).toNumber());
}
bool Announce(State& state, JS::HandleObject self, JS::HandleObject response) {
  JSContext* cx = state.cx;
  JS::RootedValue headers(cx, JS::ObjectValue(*response)), type(cx), url(cx, JS::GetReservedSlot(self, EUrl));
  std::string mime, href, lastId;
  if (!HeaderValue(state, headers, "content-type", &type) || (type.isString() && !Text(cx, type, &mime))) return false;
  mime = mime.substr(0, mime.find(';'));
  while (!mime.empty() && (mime.back() == ' ' || mime.back() == '\t')) mime.pop_back();
  for (char& c : mime) c = char(std::tolower(static_cast<unsigned char>(c)));
  if (JS::GetReservedSlot(response, 2).toInt32() != 200 || mime != "text/event-stream") {
    JS::SetReservedSlot(self, EState, JS::Int32Value(2));  // "fail the connection": no reconnection
    return Fire(state, self, "error");
  }
  JS::SetReservedSlot(self, EState, JS::Int32Value(1));
  if (!Fire(state, self, "open")) return false;
  if (ReadyState(self) != 1) return true;
  JS::RootedValue responseValue(cx, JS::ObjectValue(*response));
  const BodyRecord* body = BodyBytesOf(BodyHolderOf(responseValue));
  sse::EventStreamParser parser;
  std::vector<sse::ServerEvent> events;
  JS::RootedValue last(cx, JS::GetReservedSlot(self, ELastId));
  if (!Text(cx, last, &lastId) || !Text(cx, url, &href)) return false;
  std::u16string id, stream;
  if (!Decode(cx, lastId, &id) || !Decode(cx, body ? std::string_view(body->bytes) : std::string_view(), &stream))
    return false;
  if (!id.empty()) parser.Feed(u"id:" + id + u"\n", &events);
  parser.Feed(stream, &events);
  parser.End();
  if (auto retry = parser.TakeRetry()) JS::SetReservedSlot(self, ERetry, JS::NumberValue(*retry));
  domain::browser_environment::UrlValue parsed;
  const std::string origin = domain::browser_environment::UrlValue::Resolve(href, "", &parsed) ? parsed.origin() : "";
  for (const auto& event : events) {
    if (ReadyState(self) != 1) return true;
    JS::RootedString data(cx, JS_NewUCStringCopyN(cx, event.data.data(), event.data.size()));
    JS::RootedString id(cx, JS_NewUCStringCopyN(cx, event.lastEventId.data(), event.lastEventId.size()));
    if (!data || !id) return false;
    JS::SetReservedSlot(self, ELastId, JS::StringValue(id));
    JS::RootedValue payload(cx, JS::StringValue(data));
    if (!state.events->FireNamedMessage(self, event.type, payload, origin.c_str(), event.lastEventId)) return false;
  }
  return ReadyState(self) != 1 || Reestablish(state, self);
}
bool Finished(JSContext* cx, JS::HandleObject self, NativeFetchOutcome outcome, JS::HandleObject response,
              JS::HandleValue) {
  State& state = *static_cast<State*>(JS::GetReservedSlot(self, EOwner).toPrivate());
  if (ReadyState(self) == 2 || outcome == NativeFetchOutcome::Retired || outcome == NativeFetchOutcome::Aborted)
    return true;
  JSAutoRealm realm(cx, self);
  if (outcome == NativeFetchOutcome::Complete && response) return Announce(state, self, response);
  return Reestablish(state, self);  // a network error re-establishes the connection
}
const NativeFetchSteps kEventSourceSteps = {Finished};
}  // namespace

bool EventSourceConnect(State& state, JS::HandleObject self) {
  JSContext* cx = state.cx;
  RequestDraft draft;
  JS::RootedValue url(cx, JS::GetReservedSlot(self, EUrl)), last(cx, JS::GetReservedSlot(self, ELastId));
  std::string lastId;
  if (!Text(cx, url, &draft.url) || !Text(cx, last, &lastId)) return false;
  draft.headers = {{"Accept", "text/event-stream"}, {"Cache-Control", "no-cache"}};
  if (!lastId.empty()) draft.headers.push_back({"Last-Event-ID", lastId});
  draft.credentials = JS::GetReservedSlot(self, ECredentials).toBoolean() ? "include" : "same-origin";
  draft.cache = "no-store";
  return FetchNative(state, draft, JS::UndefinedHandleValue, self, kEventSourceSteps);
}
}  // namespace ruyijs::resource_network
