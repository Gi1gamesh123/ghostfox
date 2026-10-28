#include "runtime/resource_schema.h"
#include "runtime/resource_buffer.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include <cmath>

namespace ruyijs::resource_wire {
using resource_network::Limits;
using resource_network::Report;
static bool Headers(JSContext* cx, JS::HandleValue value, resource_network::HeaderPairs* output) {
  bool array = false;
  if (!JS::IsArrayObject(cx, value, &array)) return false;
  if (!array) return Error(cx, "Resource headers must be an array of pairs");
  JS::RootedObject list(cx, &value.toObject());
  uint32_t length;
  if (!JS::GetArrayLength(cx, list, &length)) return false;
  if (length > Limits::header_fields) return Error(cx, "Resource header field limit exceeded", true);
  size_t bytes = 0;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue entry(cx), name(cx), field(cx);
    if (!JS_GetElement(cx, list, i, &entry) || !JS::IsArrayObject(cx, entry, &array)) return false;
    if (!array) return Error(cx, "Resource header entry must be an array");
    JS::RootedObject pair(cx, &entry.toObject());
    uint32_t size;
    if (!JS::GetArrayLength(cx, pair, &size)) return false;
    if (size != 2) return Error(cx, "Resource header entry must have two items");
    std::string key, text;
    if (!JS_GetElement(cx, pair, 0, &name) || !JS_GetElement(cx, pair, 1, &field) ||
        !Text(cx, name, &key, Limits::header_bytes, TextKind::Latin1) ||
        !Text(cx, field, &text, Limits::header_bytes, TextKind::Latin1)) return false;
    bytes += key.size() + text.size();
    if (bytes > Limits::header_bytes) return Error(cx, "Resource header byte limit exceeded", true);
    if (!Report(cx, Append(output, std::move(key), std::move(text)))) return false;
  }
  return true;
}
bool Input(JSContext* cx, JS::HandleObject request, resource_network::ResourceInput* result) {
  if (!Fields(cx, request, {"id", "op", "action", "url", "body", "status", "headers"}) ||
      !Property(cx, request, "url", &result->url, Limits::key_bytes)) return false;
  JS::RootedValue value(cx);
  JS::RootedObject body(cx);
  std::string kind, encoded;
  if (!JS_GetProperty(cx, request, "body", &value) || !Object(cx, value, &body) ||
      !Fields(cx, body, {"kind", "base64"}) ||
      !Property(cx, body, "kind", &kind, 16, TextKind::ASCII)) return false;
  if (kind != "text" && kind != "bytes") return Error(cx, "Unknown resource body kind");
  result->kind = kind == "text" ? resource_network::BodyKind::Text : resource_network::BodyKind::Bytes;
  if (!Property(cx, body, "base64", &encoded, (Limits::body_bytes + 2) / 3 * 4, TextKind::ASCII) ||
      !Report(cx, DecodeBase64(encoded, &result->body))) return false;
  if (result->kind == resource_network::BodyKind::Text && !ValidUTF8(result->body))
    return Error(cx, "Resource text body must be valid UTF-8");
  bool present;
  if (!JS_HasOwnProperty(cx, request, "status", &present)) return false;
  if (present) {
    if (!JS_GetProperty(cx, request, "status", &value)) return false;
    if (!value.isNumber()) return Error(cx, "Resource status must be an integer");
    double status = value.toNumber();
    if (!std::isfinite(status) || status != std::trunc(status) || status < 100 || status > 599)
      return Error(cx, "Resource status must be between 100 and 599", true);
    result->status = int(status);
  }
  if (!JS_HasOwnProperty(cx, request, "headers", &present)) return false;
  return !present || (JS_GetProperty(cx, request, "headers", &value) && Headers(cx, value, &result->headers));
}
}
