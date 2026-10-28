// The Body mixin: text/json/arrayBuffer over the one body record, plus the
// bodyUsed/body/headers accessors. Request and Response share this file, so the
// one-shot consumption rule lives in exactly one place.
#include "resource_network/body.h"
#include "resource_network/body_result.h"
#include "resource_network/body_acquire.h"
#include "resource_network/readable_stream.h"
#include "js/Realm.h"

#include <cstring>

#include "resource_network/state.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/ArrayBuffer.h"
#include "js/experimental/TypedData.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/Array.h"
#include "js/Promise.h"
#include <memory>

namespace ruyijs::resource_network {
bool TextMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  BodyRecord* body = nullptr;
  bool consumed = false;
  // The callee slot doubles as the return value slot, so capture the owner and
  // interface before Consume may write a rejected promise there.
  State* state = FunctionOwner(args);
  const char* iface = BodyInterface(args);
  if (!ConsumeBody(cx, args, "text", &body, &consumed)) return false;
  if (state) state->Observe(iface, "text", int(ApiAccess::Call));
  if (consumed) return true;
  if (body->source!=BodySource::OwnBytes) return ConsumeStreamBody(cx,args,BodyResultKind::Text);
  JS::RootedValue value(cx);
  if (!BodyTextValue(cx, body, &value)) return false;
  return Settled(cx, value, args.rval());
}
bool JsonMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  BodyRecord* body = nullptr;
  bool consumed = false;
  State* state = FunctionOwner(args);
  const char* iface = BodyInterface(args);
  if (!ConsumeBody(cx, args, "json", &body, &consumed)) return false;
  if (state) state->Observe(iface, "json", int(ApiAccess::Call));
  if (consumed) return true;
  if (body->source!=BodySource::OwnBytes) return ConsumeStreamBody(cx,args,BodyResultKind::Json);
  JS::RootedValue parsed(cx);
  bool rejected = false;
  if (!BodyJsonValue(cx, body, &parsed, &rejected)) return false;
  if (rejected) return Rejected(cx, parsed, args.rval());
  return Settled(cx, parsed, args.rval());
}
// arrayBuffer() and bytes(): the same bytes as an ArrayBuffer or a Uint8Array over one.
bool BinaryMethod(JSContext* cx, unsigned argc, JS::Value* vp, BodyResultKind kind) {
  auto args = JS::CallArgsFromVp(argc, vp);
  BodyRecord* body = nullptr;
  bool consumed = false;
  State* state = FunctionOwner(args);
  const char* iface = BodyInterface(args);
  const char* name = kind == BodyResultKind::Bytes ? "bytes" : "arrayBuffer";
  if (!ConsumeBody(cx, args, name, &body, &consumed)) return false;
  if (state) state->Observe(iface, name, int(ApiAccess::Call));
  if (consumed) return true;
  if (body->source!=BodySource::OwnBytes) return ConsumeStreamBody(cx,args,kind);
  JS::RootedValue value(cx);
  if (!(kind == BodyResultKind::Bytes ? BodyBytesValue(cx, body, &value) : BodyArrayBufferValue(cx, body, &value)))
    return false;
  return Settled(cx, value, args.rval());
}
bool ArrayBufferMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  return BinaryMethod(cx, argc, vp, BodyResultKind::ArrayBuffer);
}
bool BytesMethod(JSContext* cx, unsigned argc, JS::Value* vp) { return BinaryMethod(cx, argc, vp, BodyResultKind::Bytes); }
bool BodyUsedGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  BodyRecord* body = BodyOf(args.thisv());
  if (!body) return Error(cx, "bodyUsed requires a Request or Response");
  if (State* state = FunctionOwner(args))
    state->Observe(BodyInterface(args), "bodyUsed", int(ApiAccess::Read));
  args.rval().setBoolean(BodyUsed(BodyHolderOf(args.thisv())));
  return true;
}
// The holder caches the sole stream and owns its body independently of Response.
bool BodyGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject holder(cx, BodyHolderOf(args.thisv()));
  BodyRecord* body = HolderBody(holder);
  if (!body) return Error(cx, "body requires a Request or Response");
  if (State* state = FunctionOwner(args))
    state->Observe(BodyInterface(args), "body", int(ApiAccess::Read));
  if (body->nullBody) { args.rval().setNull(); return true; }
  JS::RootedObject stream(cx);
  {
    JSAutoRealm realm(cx, holder);
    if (!EnsureBodyStream(cx, holder, &stream)) return false;
  }
  args.rval().setObject(*stream);
  return JS_WrapValue(cx, args.rval());
}
bool HeadersGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx, BodyReceiver(args.thisv()));
  if (!object) return Error(cx, "headers requires a Request or Response");
  if (State* state = FunctionOwner(args))
    state->Observe(BodyInterface(args), "headers", int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(object, 1));
  return JS_WrapValue(cx, args.rval());
}
const char* BodyInterface(const JS::CallArgs& args) {
  return js::GetFunctionNativeReserved(&args.callee(), 1).toInt32() ? "Response" : "Request";
}

bool BodyInput(State& state, JS::HandleValue value, BodyRecord* body) {
  if (value.isUndefined() || value.isNull()) return true;
  body->nullBody = false;
  if (value.isObject() && JS::IsArrayBufferObject(&value.toObject())) {
    size_t length = 0;
    bool shared = false;
    uint8_t* data = nullptr;
    JS::GetArrayBufferLengthAndData(&value.toObject(), &length, &shared, &data);
    body->kind = BodyKind::Bytes;
    body->bytes.assign(reinterpret_cast<const char*>(data), length);
    return true;
  }
  if (value.isObject() && JS_IsArrayBufferViewObject(&value.toObject())) {
    JSObject* view = &value.toObject();
    size_t elements = 0;
    bool shared = false;
    uint8_t* data = nullptr;
    js::GetArrayBufferViewLengthAndData(view, &elements, &shared, &data);
    (void)elements;
    (void)shared;
    body->kind = BodyKind::Bytes;
    body->bytes.assign(reinterpret_cast<const char*>(data),
                       JS_GetArrayBufferViewByteLength(view));
    return true;
  }
  body->kind = BodyKind::Text;
  return EncodeBodyText(state.cx, value, &body->bytes);
}

bool InstallBody(State& state, PageHelpers& helpers, const char* iface, JS::HandleObject prototype) {
  JSContext* cx = state.cx;
  struct Method { const char* name; JSNative native; };
  const Method methods[] = {{"text", TextMethod}, {"json", JsonMethod},
                            {"arrayBuffer", ArrayBufferMethod}, {"bytes", BytesMethod},
                            {"blob", BlobMethod}, {"formData", FormDataMethod}};
  // Slot 1 carries the interface whose prototype this install belongs to, so
  // each shared Body member reports its own Request/Response path.
  const int32_t iface_slot = strcmp(iface, "Request") == 0 ? 0 : 1;
  for (const Method& method : methods) {
    JS::RootedObject function(cx, NativeSlot(state, method.native, method.name, 0, iface_slot));
    if (!function || !JS_DefineProperty(cx, prototype, method.name, function, JSPROP_ENUMERATE) ||
        !helpers.Member(function, iface, method.name, "call")) return false;
  }
  JS::RootedObject used(cx, NativeSlot(state, BodyUsedGetter, "get bodyUsed", 0, iface_slot));
  JS::RootedObject headers(cx, NativeSlot(state, HeadersGetter, "get headers", 0, iface_slot));
  JS::RootedObject setter(cx);
  if (!used || !headers ||
      !JS_DefineProperty(cx, prototype, "bodyUsed", used, setter, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, prototype, "headers", headers, setter, JSPROP_ENUMERATE)) return false;
  // Firefox deliberately excludes body from Request (Response.webidl).
  if (iface_slot) {
    JS::RootedObject stream(cx, NativeSlot(state, BodyGetter, "get body", 0, iface_slot));
    if (!stream || !JS_DefineProperty(cx, prototype, "body", stream, setter, JSPROP_ENUMERATE) ||
        !helpers.Member(stream, iface, "body", "get")) return false;
  }
  return helpers.Member(used, iface, "bodyUsed", "get") &&
         helpers.Member(headers, iface, "headers", "get");
}

}  // namespace ruyijs::resource_network
