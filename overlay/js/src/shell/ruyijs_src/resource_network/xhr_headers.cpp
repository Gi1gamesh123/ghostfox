#include "resource_network/xhr.h"
#include <map>
namespace ruyijs::resource_network {
namespace {
std::string Lower(std::string value) {
  for (char& c : value) if (c >= 'A' && c <= 'Z') c += 'a'-'A';
  return value;
}
std::map<std::string, std::string> ResponseHeaders(const XhrRecord& r) {
  std::map<std::string, std::string> headers;
  for (const auto& [key, value] : r.response.headers) {
    if(Lower(key)=="set-cookie"||Lower(key)=="set-cookie2")continue;
    auto [it, inserted] = headers.emplace(Lower(key), value);
    if (!inserted) it->second += ", " + value;
  }
  if (!headers.count("content-length")) headers.emplace("content-length", std::to_string(r.response.body.size()));
  return headers;
}
}
bool XhrHeader(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int op = XhrSelector(args);
  JS::RootedObject self(cx);
  auto* r = XhrReceiver(cx, args, &self);
  if (!r) return false;
  const char* name = op == XGetAll ? "getAllResponseHeaders" : op == XGetHeader ? "getResponseHeader" :
                     op == XSetHeader ? "setRequestHeader" : "overrideMimeType";
  r->owner->Observe("XMLHttpRequest", name, int(ApiAccess::Call));
  if (op == XSetHeader && !r->state.writable()) return XhrException(*r->owner, "InvalidStateError");
  if (op == XOverride && r->state.ready >= 3) return XhrException(*r->owner, "InvalidStateError");
  if (op != XGetAll && argc < (op == XSetHeader ? 2u : 1u)) return Error(cx, "XHR method requires arguments");
  std::string key, value;
  if (op != XGetAll && !Convert(cx, args[0], &key)) return false;
  args.rval().setUndefined();
  if (op == XOverride) { r->mime = std::move(key); return true; }
  if (op == XSetHeader) {
    if (!Convert(cx, args[1], &value)) return false;
    if (r->requestHeaders.size() >= Limits::header_fields) return Error(cx, "Too many XHR headers", true);
    r->requestHeaders.emplace_back(std::move(key), std::move(value));
    return true;
  }
  if (r->state.ready < 2 || !r->response.status) {
    if (op == XGetAll) return Value(cx, "", args.rval());
    args.rval().setNull(); return true;
  }
  auto headers = ResponseHeaders(*r);
  if (op == XGetHeader) {
    auto it = headers.find(Lower(key));
    if (it != headers.end()) return Value(cx, it->second, args.rval());
    args.rval().setNull(); return true;
  }
  for (const auto& [name, field] : headers) value += name + ": " + field + "\r\n";
  return Value(cx, value, args.rval());
}
}
