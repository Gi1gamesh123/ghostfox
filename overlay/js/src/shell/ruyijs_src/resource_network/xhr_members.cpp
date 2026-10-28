#include "resource_network/xhr.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Wrapper.h"
namespace ruyijs::resource_network {
const char* XhrMemberName(int member) {
  static const char* names[] = {"readyState", "timeout", "withCredentials", "upload",
      "responseURL", "status", "statusText", "responseType", "response", "responseText", "responseXML",
      "mozAnon", "mozSystem"};
  return names[member];
}
bool XhrGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int member = XhrSelector(args);
  JS::RootedObject self(cx);
  auto* r = XhrReceiver(cx, args, &self);
  if (!r) return false;
  r->owner->Observe("XMLHttpRequest", XhrMemberName(member), int(ApiAccess::Read));
  switch (member) {
    case XReady: args.rval().setInt32(r->state.ready); return true;
    case XTimeout: args.rval().setNumber(r->state.timeout); return true;
    case XCredentials: args.rval().setBoolean(r->state.credentials); return true;
    case XMozAnon: args.rval().setBoolean(r->state.anonymous); return true;
    case XMozSystem: args.rval().setBoolean(false); return true;
    case XStatus: args.rval().setInt32(r->response.status); return true;
    case XStatusText: return Value(cx, r->response.status ? "OK" : "", args.rval());
    case XResponseURL: {
      JS::RootedString text(cx, DecodeBodyText(cx, r->response.finalURL));
      if (!text) return false;
      args.rval().setString(text); return true;
    }
    case XResponseType: return Value(cx, r->responseType, args.rval());
    case XUploadMember: {
      JS::RootedValue value(cx, JS::GetReservedSlot(self, XUpload));
      if (!JS_WrapValue(cx, &value)) return false;
      args.rval().set(value); return true;
    }
    default: return XhrResponse(cx, self, XhrMember(member), args.rval());
  }
}
bool XhrSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int member = XhrSelector(args);
  JS::RootedObject self(cx);
  auto* r = XhrReceiver(cx, args, &self);
  if (!r) return false;
  r->owner->Observe("XMLHttpRequest", XhrMemberName(member), int(ApiAccess::Write));
  JS::RootedValue input(cx, args.get(0));
  if (member == XTimeout) {
    r->state.timeout = input.isNumber() ? JS::ToUint32(input.toNumber()) : 0;
  } else if (member == XCredentials) {
    if (r->state.ready > 1 || r->state.pending || r->state.anonymous)
      return XhrException(*r->owner, "InvalidStateError");
    r->state.credentials = JS::ToBoolean(input);
  } else {
    if (r->state.ready >= 3) return XhrException(*r->owner, "InvalidStateError");
    std::string type;
    if (!input.isSymbol() && !EncodeBodyText(cx, input, &type)) return false;
    if (type.empty() || type == "text" || type == "json" || type == "document" ||
        type == "arraybuffer" || type == "blob") r->responseType = std::move(type);
  }
  args.rval().setUndefined(); return true;
}
}
