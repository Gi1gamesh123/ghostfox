// The Response object itself: its reserved-slot layout, the prototype field
// getters and clone(), and the factory the fetch entry uses. The constructor and
// the static methods live in response.cpp; both build objects through
// ResponseNew here, so the slot layout has exactly one owner.
#include "resource_network/state.h"
#include "resource_network/body.h"
#include "resource_network/model_body_bytes.h"
#include "resource_network/page_http.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <memory>
#include <new>

namespace ruyijs::resource_network {
namespace {
enum ResponseField { StatusField, StatusTextField, OkField, UrlField, TypeField, RedirectedField };

// BodyOf accepts Request too (the shared mixin), so the Response members keep
// their own receiver brand check.
bool IsResponse(JS::HandleValue value) {
  return value.isObject() && JS::GetClass(&value.toObject()) == &kResponseClass;
}
// The installed member name of each readable Response field; static names only.
const char* ResponseMemberName(ResponseField field) {
  switch (field) {
    case StatusField: return "status";
    case StatusTextField: return "statusText";
    case OkField: return "ok";
    case UrlField: return "url";
    case TypeField: return "type";
    case RedirectedField: return "redirected";
  }
  return "";
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!IsResponse(args.thisv())) return Error(cx, "Response member requires a Response");
  JS::RootedObject self(cx, &args.thisv().toObject());
  const int32_t field = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  if (State* state = FunctionOwner(args))
    state->Observe("Response", ResponseMemberName(ResponseField(field)), int(ApiAccess::Read));
  switch (ResponseField(field)) {
    case StatusField:
      args.rval().set(JS::GetReservedSlot(self, 2));
      return true;
    case StatusTextField:
      args.rval().set(JS::GetReservedSlot(self, 3));
      return true;
    case OkField: {
      const int32_t status = JS::GetReservedSlot(self, 2).toInt32();
      args.rval().setBoolean(status >= 200 && status <= 299);
      return true;
    }
    case UrlField:
      args.rval().set(JS::GetReservedSlot(self, 4));
      return true;
    case TypeField:
      args.rval().set(JS::GetReservedSlot(self, 5));
      return true;
    default:
      args.rval().set(JS::GetReservedSlot(self, 6));
      return true;
  }
}

// clone() copies the body and the metadata; the copy is unconsumed even when the
// original already was consumed, and a consumed body is a synchronous TypeError.
bool CloneCall(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);return ResponseClone(cx,args);
}

}  // namespace

const JSClass kResponseClass = {"Response",
                                JSCLASS_HAS_RESERVED_SLOTS(7)};

bool ResponseNew(State& state, JS::HandleObject prototype, BodyRecord* body,
                 JS::HandleObject headers, int32_t status, JS::HandleString status_text,
                 JS::HandleString url, const char* type, bool redirected,
                 JS::MutableHandleObject result) {
  JSContext* cx = state.cx;
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kResponseClass, prototype));
  JS::RootedString kind(cx, JS_NewStringCopyZ(cx, type));
  JS::RootedObject holder(cx, NewBodyHolder(state));
  if (!object || !kind || !holder) return false;
  // No fallible work follows: the caller releases its unique_ptr after success.
  CommitBodyHolder(holder, body);
  JS::SetReservedSlot(object, 0, JS::ObjectValue(*holder));
  JS::SetReservedSlot(object, 1, JS::ObjectValue(*headers));
  JS::SetReservedSlot(object, 2, JS::Int32Value(status));
  JS::SetReservedSlot(object, 3, JS::StringValue(status_text));
  JS::SetReservedSlot(object, 4, JS::StringValue(url));
  JS::SetReservedSlot(object, 5, JS::StringValue(kind));
  JS::SetReservedSlot(object, 6, JS::BooleanValue(redirected));
  result.set(object);
  return true;
}

bool ResponseObject(State& state, const HttpResult& http, const std::string& url,
                    const std::string& method, const std::string& mode,
                    bool credentialsInclude,
                    JS::MutableHandleObject result) {
  JSContext* cx = state.cx;
  const bool opaque = !http.opaqueRedirect && http.crossOrigin && mode == "no-cors";
  const bool filtered = opaque || http.opaqueRedirect;
  const char* responseType = http.opaqueRedirect ? "opaqueredirect" :
      opaque ? "opaque" : http.crossOrigin && mode == "cors" ? "cors" : "basic";
  JS::RootedObject headers(cx);
  HeaderPairs visible;
  if (!filtered && !Report(cx, FilterResponseHeaders(http.headers,http.crossOrigin,mode,
                                                      credentialsInclude,&visible))) return false;
  if (!HeaderObject(state, visible, &headers, true)) return false;
  std::unique_ptr<BodyRecord> body(new (std::nothrow) BodyRecord());
  const int32_t status = opaque ? 0 : http.status;
  static const std::string empty;
  const auto& statusText = filtered ? empty : http.statusText;
  JS::RootedString status_text(cx, JS_NewStringCopyZ(cx, statusText.c_str()));
  const auto& finalURL = http.finalURL.empty() ? url : http.finalURL;
  const auto& responseURL = filtered ? empty : finalURL;
  JS::RootedString address(cx, JS_NewStringCopyZ(cx, responseURL.c_str()));
  if (!body || !status_text || !address) { JS_ReportOutOfMemory(cx); return false; }
  body->nullBody = filtered || method == "HEAD";
  if (!body->nullBody && !Report(cx,CopyBodyBytes(http.body,&body->bytes))) return false;
  body->kind = http.kind;
  if (!ResponseNew(state, state.responsePrototype, body.get(), headers, status, status_text,
                   address, responseType, filtered ? false : http.redirected, result)) return false;
  body.release();
  return true;
}

bool InstallResponseObject(State& state, PageHelpers& helpers, JS::HandleObject prototype) {
  JSContext* cx = state.cx;
  JS::RootedObject clone(cx, NativeFunction(state, CloneCall, "clone", 0));
  if (!clone || !JS_DefineProperty(cx, prototype, "clone", clone, JSPROP_ENUMERATE) ||
      !helpers.Member(clone, "Response", "clone", "call")) return false;
  struct Field { const char* name; ResponseField field; };
  const Field fields[] = {{"status", StatusField}, {"statusText", StatusTextField},
                          {"ok", OkField}, {"url", UrlField}, {"type", TypeField},
                          {"redirected", RedirectedField}};
  JS::RootedObject setter(cx);
  for (const Field& field : fields) {
    JS::RootedObject getter(cx, NativeSlot(state, Get, field.name, 0, field.field));
    if (!getter ||
        !JS_DefineProperty(cx, prototype, field.name, getter, setter, JSPROP_ENUMERATE) ||
        !helpers.Member(getter, "Response", field.name, "get")) return false;
  }
  return InstallBody(state, helpers, "Response", prototype);
}
}  // namespace ruyijs::resource_network
