#include "web_geometry/rect_list.h"
#include "js/PropertyDescriptor.h"
#include "js/String.h"
#include "jsfriendapi.h"

namespace ruyijs::web_geometry {
const JSClass kRectListClass = PROXY_CLASS_DEF("DOMRectList", JSCLASS_HAS_RESERVED_SLOTS(1));
namespace {
char rectListFamily;
bool Index(JSContext* cx, JS::HandleId id, uint32_t* index, bool* indexed) {
  *indexed = id.isInt();
  if (*indexed) { *index = uint32_t(id.toInt()); return true; }
  if (!id.isString()) return true;
  JSLinearString* text = JS_EnsureLinearString(cx, id.toString());
  if (!text) return false;
  *indexed = js::StringIsArrayIndex(text, index) && *index != UINT32_MAX;
  return true;
}
// The reserved slot owns a private array of snapshot rectangles. The private
// object owns ordinary expando properties; neither object retains DOM nodes.
class Handler final : public js::NurseryAllocableProxyHandler {
 public:
  constexpr Handler() : NurseryAllocableProxyHandler(&rectListFamily) {}
  bool getOwnPropertyDescriptor(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
      JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> desc) const override {
    uint32_t index = 0; bool indexed;
    if (!Index(cx, id, &index, &indexed)) return false;
    if (indexed) {
      desc.reset();
      if (index >= RectListLength(cx,proxy)) return true;
      JS::RootedValue rect(cx);
      if (!RectListItem(cx,proxy,index,&rect)) return false;
      desc.set(mozilla::Some(JS::PropertyDescriptor::Data(rect,
          {JS::PropertyAttribute::Enumerable, JS::PropertyAttribute::Configurable})));
      return true;
    }
    JS::RootedObject expando(cx, &js::GetProxyPrivate(proxy).toObject());
    return JS_GetOwnPropertyDescriptorById(cx, expando, id, desc);
  }
  bool defineProperty(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
      JS::Handle<JS::PropertyDescriptor> desc, JS::ObjectOpResult& result) const override {
    uint32_t index = 0; bool indexed;
    if (!Index(cx, id, &index, &indexed)) return false;
    if (indexed) return result.succeed();
    JS::RootedObject expando(cx, &js::GetProxyPrivate(proxy).toObject());
    return JS_DefinePropertyById(cx, expando, id, desc, result);
  }
  bool ownPropertyKeys(JSContext* cx, JS::HandleObject proxy,
                       JS::MutableHandleIdVector keys) const override {
    const auto length=RectListLength(cx,proxy);
    for(uint32_t i=0;i<length;++i)
      if (!keys.append(JS::PropertyKey::Int(i))) { JS_ReportOutOfMemory(cx); return false; }
    JS::RootedObject expando(cx, &js::GetProxyPrivate(proxy).toObject());
    JS::RootedIdVector extra(cx);
    if (!js::GetPropertyKeys(cx, expando, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &extra)) return false;
    if (!keys.appendAll(extra)) { JS_ReportOutOfMemory(cx); return false; }
    return true;
  }
  bool delete_(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
                JS::ObjectOpResult& result) const override {
    uint32_t index = 0; bool indexed;
    if (!Index(cx, id, &index, &indexed)) return false;
    if (indexed) return index >= RectListLength(cx,proxy) ? result.succeed() : result.failCantDelete();
    JS::RootedObject expando(cx, &js::GetProxyPrivate(proxy).toObject());
    return JS_DeletePropertyById(cx, expando, id, result);
  }
  bool set(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
      JS::HandleValue value, JS::HandleValue receiver, JS::ObjectOpResult& result) const override {
    uint32_t index = 0; bool indexed;
    if (!Index(cx, id, &index, &indexed)) return false;
    if (indexed) return result.succeed();
    return BaseProxyHandler::set(cx, proxy, id, value, receiver, result);
  }
  bool getPrototypeIfOrdinary(JSContext* cx, JS::HandleObject proxy, bool* ordinary,
                              JS::MutableHandleObject proto) const override {
    *ordinary = true;
    return JS_GetPrototype(cx, proxy, proto);
  }
  bool preventExtensions(JSContext*, JS::HandleObject, JS::ObjectOpResult& result) const override {
    return result.failCantPreventExtensions();
  }
  bool isExtensible(JSContext*, JS::HandleObject, bool* extensible) const override {
    *extensible = true; return true;
  }
  const char* className(JSContext*, JS::HandleObject) const override { return "DOMRectList"; }
};
const Handler handler;
}
const js::BaseProxyHandler* RectListHandler() { return &handler; }
}
