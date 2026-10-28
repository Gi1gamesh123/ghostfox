#include "runtime/cookie_control.h"
#include "runtime/resource_schema.h"
#include "resource_network/model_storage.h"
#include "resource_network/page_http.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs {
namespace {
using namespace resource_wire;
bool StringField(JSContext* cx, JS::HandleObject object, const char* key,
                 const std::string& bytes) {
  // A Latin-1 projection preserves every jar byte, including non-UTF-8 headers.
  JS::RootedString value(cx, JS_NewStringCopyN(cx, bytes.data(), bytes.size()));
  return value && JS_DefineProperty(cx, object, key, value, JSPROP_ENUMERATE);
}
bool BooleanField(JSContext* cx, JS::HandleObject object, const char* key, bool flag) {
  JS::RootedValue value(cx, JS::BooleanValue(flag));
  return JS_DefineProperty(cx, object, key, value, JSPROP_ENUMERATE);
}
bool Snapshot(JSContext* cx, resource_network::StorageArea* store, double now,
              JS::MutableHandleValue result) {
  std::vector<resource_network::CookieRecord> cookies;
  if (!resource_network::Report(cx, resource_network::CookieSnapshot(store, now, &cookies)))
    return false;
  JS::RootedObject list(cx, JS::NewArrayObject(cx, cookies.size()));
  if (!list) return false;
  for (size_t i = 0; i < cookies.size(); ++i) {
    const auto& cookie = cookies[i];
    JS::RootedObject row(cx, JS_NewPlainObject(cx));
    JS::RootedValue expires(cx, cookie.session ? JS::NullValue() : JS::DoubleValue(cookie.expires));
    if (!row || !StringField(cx, row, "name", cookie.name) ||
        !StringField(cx, row, "value", cookie.value) ||
        !StringField(cx, row, "domain", cookie.domain) ||
        !StringField(cx, row, "path", cookie.path) ||
        !StringField(cx, row, "sameSite", cookie.sameSite) ||
        !BooleanField(cx, row, "hostOnly", cookie.hostOnly) ||
        !BooleanField(cx, row, "secure", cookie.secure) ||
        !BooleanField(cx, row, "httpOnly", cookie.httpOnly) ||
        !BooleanField(cx, row, "session", cookie.session) ||
        !JS_DefineProperty(cx, row, "expires", expires, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, row, "created", cookie.created, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, row, "order", double(cookie.order), JSPROP_ENUMERATE) ||
        !JS_DefineElement(cx, list, i, row, JSPROP_ENUMERATE)) return false;
  }
  result.setObject(*list);
  return true;
}
bool Address(JSContext* cx, const std::string& input, std::string* normalized) {
  if (!resource_network::ResolveURL("", input, normalized) ||
      (normalized->compare(0, 7, "http://") != 0 && normalized->compare(0, 8, "https://") != 0))
    return Error(cx, "Cookie URL must be absolute HTTP(S)");
  return true;
}
bool Header(JSContext* cx, JS::HandleObject request, resource_network::StorageArea* store,
            double now, JS::MutableHandleValue result) {
  std::string url, source, method, header;
  JS::RootedValue navigation(cx);
  if (!Fields(cx, request, {"id", "op", "action", "url", "source_url", "navigation", "method"}) ||
      !Property(cx, request, "url", &url, resource_network::Limits::key_bytes) ||
      !Address(cx, url, &url) ||
      !Property(cx, request, "source_url", &source, resource_network::Limits::key_bytes) ||
      (!source.empty() && !Address(cx, source, &source)) ||
      !Property(cx, request, "method", &method, 64, TextKind::ASCII) ||
      !JS_GetProperty(cx, request, "navigation", &navigation)) return false;
  if (!navigation.isBoolean()) return Error(cx, "Cookie navigation must be a boolean");
  if (method.empty()) return Error(cx, "Cookie method must be an HTTP token");
  for (char& c : method) {
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || std::string_view("!#$%&'*+-.^_`|~").find(c) != std::string_view::npos))
      return Error(cx, "Cookie method must be an HTTP token");
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
  }
  if (!resource_network::Report(cx, resource_network::CookieJoined(store, url, now, true,
      &header, source, navigation.toBoolean(), method == "GET" || method == "HEAD"))) return false;
  JS::RootedString value(cx, JS_NewStringCopyN(cx, header.data(), header.size()));
  if (!value) return false;
  result.setString(value);
  return true;
}
}
bool CookieRequest(JSContext* cx, resource_network::Model& owner, JS::HandleObject request,
                   JS::MutableHandleValue result) {
  using namespace resource_wire;
  std::string action;
  if (!Property(cx, request, "action", &action, 16, TextKind::ASCII)) return false;
  if (action != "export" && action != "header") return Error(cx, "Unknown cookie action");
  if (action == "export" && !Fields(cx, request, {"id", "op", "action"})) return false;
  if (!owner.Cookies()) return Error(cx, "Cookie store is unavailable");
  resource_network::HttpContext context;
  if (!owner.ReadHttpContext(&context)) return false;
  return action == "export" ? Snapshot(cx, owner.Cookies(), context.now, result) :
      Header(cx, request, owner.Cookies(), context.now, result);
}
}
