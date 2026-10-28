#include "web_platform/object_url_remote.h"
#include "web_platform/object_url_wire.h"
#include "web_platform/state.h"
#include "jsapi.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/WeakMap.h"

namespace ruyijs::web_platform {
namespace wire = object_url_wire;
namespace {
bool Exchange(State& state, const wire::Request& request, wire::Reply* result) {
  std::string frame, reply;
  if (!wire::Encode(request, &frame))
    return WebKit::FailKind(state.cx, "Object URL request exceeds the bounded wire limits", JSEXN_RANGEERR);
  if (!state.objectUrlExchange(state.objectUrlExchangeContext, frame, &reply)) return false;
  if (reply.size() != 1) return WebKit::Fail(state.cx, "Invalid object URL service reply");
  *result = wire::Reply(reply[0]);
  if (*result == wire::Reply::Ok) return true;
  if (*result == wire::Reply::NeedBody) {
    if (request.operation == wire::Operation::Offer) return true;
    return WebKit::Fail(state.cx, "Unexpected object URL body request");
  }
  return WebKit::FailKind(state.cx, "Object URL request failed",
                         *result == wire::Reply::Limit ? JSEXN_RANGEERR : JSEXN_TYPEERR);
}
bool Reference(State& state, JS::HandleObject object, uint32_t* reference) {
  JSContext* cx = state.cx;
  if (!state.objectUrlReferences) state.objectUrlReferences = JS::NewWeakMapObject(cx);
  if (!state.objectUrlReferences) return false;
  JS::RootedValue key(cx, JS::ObjectValue(*object)), value(cx);
  if (!JS::GetWeakMapEntry(cx, state.objectUrlReferences, key, &value)) return false;
  if (value.isUndefined()) {
    if (state.nextObjectUrlReference == UINT32_MAX)
      return WebKit::FailKind(cx, "Worker Blob reference limit exceeded", JSEXN_RANGEERR);
    value.setNumber(++state.nextObjectUrlReference);
    if (!JS::SetWeakMapEntry(cx, state.objectUrlReferences, key, value)) return false;
  }
  *reference = uint32_t(value.toNumber());
  return true;
}
}
bool RetainRemoteObjectUrl(State& state, JS::HandleObject object, const std::string& url) {
  uint32_t reference;
  if (!Reference(state, object, &reference)) return false;
  wire::Request request{wire::Operation::Offer, reference, url, {}, {}};
  wire::Reply reply;
  if (!Exchange(state, request, &reply)) return false;
  if (reply == wire::Reply::Ok) return true;
  JS::RootedValue stored(state.cx, JS::GetReservedSlot(object, kBlobTypeSlot));
  std::string type, bytes;
  if (!Convert(state.cx, stored, &type)) return false;
  const uint8_t* data = nullptr;
  size_t length = 0;
  if (!BlobBytes(state.cx, object, &data, &length)) return false;
  if (length) bytes.assign(reinterpret_cast<const char*>(data), length);
  request.operation = wire::Operation::Body;
  request.type = type; request.bytes = bytes;
  return Exchange(state, request, &reply) && reply == wire::Reply::Ok;
}
bool RevokeRemoteObjectUrl(State& state, const std::string& url) {
  // An overlong URL cannot match a registered URL, but conversion still ran.
  if (url.size() > wire::kUrlLimit) return true;
  wire::Reply reply;
  return Exchange(state, {wire::Operation::Revoke, 0, url, {}, {}}, &reply) && reply == wire::Reply::Ok;
}
bool ReadRemoteObjectUrl(State& state, const std::string& url, std::string* bytes, std::string* type) {
  wire::Request request{wire::Operation::Read, 0, url, {}, {}};
  std::string frame, reply;
  if (!wire::Encode(request, &frame) || !state.objectUrlExchange ||
      !state.objectUrlExchange(state.objectUrlExchangeContext, frame, &reply)) return false;
  if (reply.size() < 5 || reply[0] != char(wire::Reply::Ok)) return false;
  const uint32_t length = wire::Get(reply, 1);
  if (reply.size() - 5 < length) return false;
  type->assign(reply, 5, length);
  bytes->assign(reply, 5 + length, std::string::npos);
  return true;
}
}
