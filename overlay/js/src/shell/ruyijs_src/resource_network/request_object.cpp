// The Request object: its reserved-slot layout, the field getters, clone() and
// the factory that `new Request` builds through. The argument normalization
// lives in request.cpp; both files write the same RequestDraft, so the layout
// has exactly one owner and the fetch entry reads the same description.
#include "resource_network/state.h"
#include "resource_network/request.h"
#include "resource_network/abort.h"
#include "resource_network/body.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <memory>
#include <new>

namespace ruyijs::resource_network {
namespace {
// Slot 0 is the BodyRecord, slot 1 the Headers object the Body mixin exposes;
// the rest are the readable request fields.
enum RequestSlot {
  kBodySlot = 0, kHeadersSlot = 1, kMethodSlot = 2, kUrlSlot = 3, kModeSlot = 4,
  kCredentialsSlot = 5, kCacheSlot = 6, kRedirectSlot = 7, kSignalSlot = 8,
  kReferrerSlot = 9, kReferrerPolicySlot = 10, kIntegritySlot = 11, kKeepaliveSlot = 12,
  kDestinationSlot = 13, kRequestSlots
};

bool IsRequest(JS::HandleValue value) {
  return value.isObject() && JS::GetClass(&value.toObject()) == &kRequestClass;
}
// The installed member name of each readable Request field; static names only.
const char* RequestMemberName(int32_t slot) {
  switch (slot) {
    case kMethodSlot: return "method";
    case kUrlSlot: return "url";
    case kModeSlot: return "mode";
    case kCredentialsSlot: return "credentials";
    case kCacheSlot: return "cache";
    case kRedirectSlot: return "redirect";
    case kReferrerSlot: return "referrer";
    case kReferrerPolicySlot: return "referrerPolicy";
    case kIntegritySlot: return "integrity";
    case kKeepaliveSlot: return "keepalive";
    case kDestinationSlot: return "destination";
    default: return "";
  }
}
// The member tables pass the slot they read back through the function's second
// reserved slot, so one getter serves every field.
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!IsRequest(args.thisv())) return Error(cx, "Request member requires a Request");
  JS::RootedObject self(cx, &args.thisv().toObject());
  const int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  if (State* state = FunctionOwner(args))
    state->Observe("Request", RequestMemberName(slot), int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(self, slot));
  return true;
}
// A Request's own signal is created the first time it is read: the interaction
// tree roots every attached target until the realm ends, so the common
// fetch(new Request(...)) path must not allocate one it never uses.
bool SignalGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionOwner(args);
  if (!IsRequest(args.thisv())) return Error(cx, "Request member requires a Request");
  state.Observe("Request", "signal", int(ApiAccess::Read));
  JS::RootedObject self(cx, &args.thisv().toObject());
  JS::RootedValue signal(cx, JS::GetReservedSlot(self, kSignalSlot));
  if (signal.isObject()) {
    args.rval().set(signal);
    return true;
  }
  JS::RootedObject created(cx);
  if (!NewAbortSignal(state, &created)) return false;
  JS::SetReservedSlot(self, kSignalSlot, JS::ObjectValue(*created));
  args.rval().setObject(*created);
  return true;
}
bool CloneCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionOwner(args);
  JS::RootedObject self(cx, BodyReceiver(args.thisv()));
  if (!self || JS::GetClass(self) != &kRequestClass)
    return BodyOperationError(cx, "Request", "clone", true);
  state.Observe("Request", "clone", int(ApiAccess::Call));
  BodyRecord* body = BodyOf(self);
  if (body && (body->used || BodyLocked(BodyHolderOf(args.thisv()))))
    return BodyOperationError(cx, "Request", "clone", false);
  RequestDraft draft;
  JS::RootedValue signal(cx);
  if (!ReadRequest(state, self, &draft, &signal)) return false;
  JS::RootedObject object(cx);
  if (!RequestNew(state, state.requestPrototype, draft, signal, &object)) return false;
  args.rval().setObject(*object);
  return true;
}
}  // namespace

const JSClass kRequestClass = {"Request",
                               JSCLASS_HAS_RESERVED_SLOTS(kRequestSlots)};

bool ReadRequest(State& state, JS::HandleObject source, RequestDraft* draft,
                 JS::MutableHandleValue signal) {
  JSContext* cx = state.cx;
  BodyRecord* body = BodyOf(source);
  if (body) {
    draft->body = body->bytes;
    draft->kind = body->kind;
    draft->nullBody = body->nullBody;
  }
  auto read = [&](int32_t slot, std::string* text) {
    JS::RootedValue value(cx, JS::GetReservedSlot(source, slot));
    return EncodeBodyText(cx, value, text);
  };
  JS::RootedValue keepalive(cx, JS::GetReservedSlot(source, kKeepaliveSlot));
  draft->keepalive = JS::ToBoolean(keepalive);
  signal.set(JS::GetReservedSlot(source, kSignalSlot));
  JS::RootedValue headers(cx, JS::GetReservedSlot(source, kHeadersSlot));
  JS::RootedValue integrity(cx, JS::GetReservedSlot(source, kIntegritySlot));
  return read(kMethodSlot, &draft->method) && read(kUrlSlot, &draft->url) &&
         read(kModeSlot, &draft->mode) && read(kCredentialsSlot, &draft->credentials) &&
         read(kCacheSlot, &draft->cache) && read(kRedirectSlot, &draft->redirect) &&
         read(kReferrerSlot, &draft->referrer) &&
         read(kReferrerPolicySlot, &draft->referrer_policy) &&
         RequestIntegrity(cx, integrity, &draft->integrity) &&
         HeaderPairsArgument(cx, headers, &draft->headers);
}

bool RequestNew(State& state, JS::HandleObject prototype, const RequestDraft& draft,
                JS::HandleValue signal, JS::MutableHandleObject result) {
  JSContext* cx = state.cx;
  std::unique_ptr<BodyRecord> body(new (std::nothrow) BodyRecord());
  if (!body) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  body->bytes = draft.body;
  body->kind = draft.kind;
  body->nullBody = draft.nullBody;
  // Every fallible step happens before the first reserved slot is written, so a
  // failure leaves the owned record to this frame and never to the finalizer.
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kRequestClass, prototype));
  JS::RootedObject headers(cx);
  JS::RootedObject signal_object(cx);
  JS::RootedString method(cx, JS_NewStringCopyN(cx, draft.method.data(), draft.method.size()));
  JS::RootedString url(cx, DecodeBodyText(cx, draft.url));
  JS::RootedString mode(cx, JS_NewStringCopyN(cx, draft.mode.data(), draft.mode.size()));
  JS::RootedString credentials(
      cx, JS_NewStringCopyN(cx, draft.credentials.data(), draft.credentials.size()));
  JS::RootedString cache(cx, JS_NewStringCopyN(cx, draft.cache.data(), draft.cache.size()));
  JS::RootedString redirect(
      cx, JS_NewStringCopyN(cx, draft.redirect.data(), draft.redirect.size()));
  JS::RootedString referrer(cx, DecodeBodyText(cx, draft.referrer));
  JS::RootedString policy(
      cx, JS_NewStringCopyN(cx, draft.referrer_policy.data(), draft.referrer_policy.size()));
  JS::RootedString integrity(
      cx, JS_NewUCStringCopyN(cx, draft.integrity.data(), draft.integrity.size()));
  JS::RootedString destination(cx, JS_GetEmptyString(cx));
  if (!object || !HeaderObject(state, draft.headers, &headers) || !method || !url || !mode ||
      !credentials || !cache || !redirect || !referrer || !policy || !integrity) return false;
  JS::RootedObject holder(cx, NewBodyHolder(state));
  if (!holder) return false;
  if (signal.isObject()) signal_object = &signal.toObject();
  // Publish ownership only after the last fallible allocation.
  CommitBodyHolder(holder, body.get());
  JS::SetReservedSlot(object, kBodySlot, JS::ObjectValue(*holder));
  JS::SetReservedSlot(object, kHeadersSlot, JS::ObjectValue(*headers));
  JS::SetReservedSlot(object, kMethodSlot, JS::StringValue(method));
  JS::SetReservedSlot(object, kUrlSlot, JS::StringValue(url));
  JS::SetReservedSlot(object, kModeSlot, JS::StringValue(mode));
  JS::SetReservedSlot(object, kCredentialsSlot, JS::StringValue(credentials));
  JS::SetReservedSlot(object, kCacheSlot, JS::StringValue(cache));
  JS::SetReservedSlot(object, kRedirectSlot, JS::StringValue(redirect));
  JS::SetReservedSlot(object, kSignalSlot,
                      signal_object ? JS::ObjectValue(*signal_object) : JS::UndefinedValue());
  JS::SetReservedSlot(object, kReferrerSlot, JS::StringValue(referrer));
  JS::SetReservedSlot(object, kReferrerPolicySlot, JS::StringValue(policy));
  JS::SetReservedSlot(object, kIntegritySlot, JS::StringValue(integrity));
  JS::SetReservedSlot(object, kKeepaliveSlot, JS::BooleanValue(draft.keepalive));
  JS::SetReservedSlot(object, kDestinationSlot, JS::StringValue(destination));
  body.release();
  result.set(object);
  return true;
}

bool InstallRequestObject(State& state, PageHelpers& helpers, JS::HandleObject prototype) {
  JSContext* cx = state.cx;
  JS::RootedObject clone(cx, NativeFunction(state, CloneCall, "clone", 0));
  JS::RootedObject signal(cx, NativeFunction(state, SignalGet, "get signal", 0));
  if (!clone || !signal ||
      !JS_DefineProperty(cx, prototype, "clone", clone, JSPROP_ENUMERATE) ||
      !helpers.Member(clone, "Request", "clone", "call")) return false;
  JS::RootedObject setter(cx);
  if (!JS_DefineProperty(cx, prototype, "signal", signal, setter, JSPROP_ENUMERATE) ||
      !helpers.Member(signal, "Request", "signal", "get")) return false;
  struct Field { const char* name; int32_t slot; };
  const Field fields[] = {{"method", kMethodSlot},        {"url", kUrlSlot},
                          {"destination", kDestinationSlot},
                          {"mode", kModeSlot},            {"credentials", kCredentialsSlot},
                          {"cache", kCacheSlot},          {"redirect", kRedirectSlot},
                          {"referrer", kReferrerSlot},
                          {"referrerPolicy", kReferrerPolicySlot},
                          {"integrity", kIntegritySlot},  {"keepalive", kKeepaliveSlot}};
  for (const Field& field : fields) {
    const std::string name = std::string("get ") + field.name;
    JS::RootedObject getter(cx, NativeSlot(state, Get, name.c_str(), 0, field.slot));
    if (!getter ||
        !JS_DefineProperty(cx, prototype, field.name, getter, setter, JSPROP_ENUMERATE) ||
        !helpers.Member(getter, "Request", field.name, "get")) return false;
  }
  return InstallBody(state, helpers, "Request", prototype);
}
}  // namespace ruyijs::resource_network
