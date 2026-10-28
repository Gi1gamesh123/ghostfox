// Unique XHR-family installer. Instances and delivery live in this same owner.
#include "resource_network/xhr.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::resource_network {
namespace {
bool Accessor(State& s, PageHelpers& helpers, JS::HandleObject proto, const char* iface,
              const char* name, int slot, bool handler, bool writable) {
  std::string getName = std::string("get ") + name, setName = std::string("set ") + name;
  JS::RootedObject get(s.cx, NativeSlot(s, handler ? XhrHandlerGet : XhrGet, getName.c_str(), 0, slot));
  JS::RootedObject set(s.cx);
  if (writable) set = NativeSlot(s, handler ? XhrHandlerSet : XhrSet, setName.c_str(), 1, slot);
  return get && (!writable || set) &&
      JS_DefineProperty(s.cx, proto, name, get, set, JSPROP_ENUMERATE) &&
      helpers.Member(get, iface, name, "get") && (!set || helpers.Member(set, iface, name, "set"));
}
bool Finish(State& s, PageHelpers& helpers, const char* name,
            JS::HandleObject constructor, JS::HandleObject proto) {
  return JS_DefineProperty(s.cx, constructor, "prototype", proto, JSPROP_READONLY | JSPROP_PERMANENT) &&
      JS_DefineProperty(s.cx, proto, "constructor", constructor, 0) && Tag(s.cx, proto, name) &&
      JS_DefineProperty(s.cx, s.page, name, constructor, 0) && helpers.Constructor(name, constructor);
}
}
bool InstallXhr(State& s, PageHelpers& helpers) {
  JSContext* cx = s.cx;
  JS::RootedObject target(cx, s.events->prototype());
  if (!JS_GetProperty(cx, target, "addEventListener", &s.xhrAddListener) ||
      !JS_GetProperty(cx, target, "removeEventListener", &s.xhrRemoveListener) ||
      !JS_GetProperty(cx, s.page, "DOMException", &s.xhrException)) return false;
  JS::RootedObject base(cx, JS_NewObjectWithGivenProto(cx, nullptr, target));
  JS::RootedObject baseCtor(cx, NativeFunction(s, XhrIllegalConstructor, "XMLHttpRequestEventTarget", 0, JSFUN_CONSTRUCTOR));
  if (!base || !baseCtor) return false;
  for (int slot = XLoadStart; slot <= XLoadEnd; ++slot)
    if (!Accessor(s, helpers, base, "XMLHttpRequestEventTarget", XhrHandlerName(slot), slot, true, true)) return false;
  if (!Finish(s, helpers, "XMLHttpRequestEventTarget", baseCtor, base)) return false;
  s.xhrUploadPrototype = JS_NewObjectWithGivenProto(cx, nullptr, base);
  JS::RootedObject uploadCtor(cx, NativeFunction(s, XhrIllegalConstructor, "XMLHttpRequestUpload", 0, JSFUN_CONSTRUCTOR));
  if (!s.xhrUploadPrototype || !uploadCtor ||
      !Finish(s, helpers, "XMLHttpRequestUpload", uploadCtor, s.xhrUploadPrototype)) return false;
  s.xhrPrototype = JS_NewObjectWithGivenProto(cx, nullptr, base);
  JS::RootedObject ctor(cx, NativeFunction(s, XhrConstructor, "XMLHttpRequest", 0, JSFUN_CONSTRUCTOR));
  if (!s.xhrPrototype || !ctor) return false;
  const char* constants[] = {"UNSENT", "OPENED", "HEADERS_RECEIVED", "LOADING", "DONE"};
  for (int i = 0; i < 5; ++i) {
    unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
    if (!JS_DefineProperty(cx, ctor, constants[i], i, flags) ||
        !JS_DefineProperty(cx, s.xhrPrototype, constants[i], i, flags)) return false;
  }
  if (!Accessor(s, helpers, s.xhrPrototype, "XMLHttpRequest", "onreadystatechange", XReadyHandler, true, true)) return false;
  for (int member = XReady; member <= (s.workerScope ? XText : XXml); ++member)
    if (!Accessor(s, helpers, s.xhrPrototype, "XMLHttpRequest", XhrMemberName(member), member,
                  false, member == XTimeout || member == XCredentials || member == XResponseType)) return false;
  for (int member : {XMozAnon, XMozSystem})
    if (!Accessor(s, helpers, s.xhrPrototype, "XMLHttpRequest", XhrMemberName(member), member, false, false))
      return false;
  struct Method { const char* name; unsigned length; XhrOperation op; JSNative callback; };
  const Method methods[] = {{"abort",0,XAbortCall,XhrRequest}, {"getAllResponseHeaders",0,XGetAll,XhrHeader},
      {"getResponseHeader",1,XGetHeader,XhrHeader}, {"open",2,XOpen,XhrRequest},
      {"overrideMimeType",1,XOverride,XhrHeader}, {"send",0,XSend,XhrRequest},
      {"setRequestHeader",2,XSetHeader,XhrHeader}};
  for (const auto& item : methods) {
    JS::RootedObject method(cx, NativeSlot(s, item.callback, item.name, item.length, item.op));
    if (!method || !JS_DefineProperty(cx, s.xhrPrototype, item.name, method, JSPROP_ENUMERATE) ||
        !helpers.Member(method, "XMLHttpRequest", item.name, "call")) return false;
  }
  return Finish(s, helpers, "XMLHttpRequest", ctor, s.xhrPrototype);
}
}
