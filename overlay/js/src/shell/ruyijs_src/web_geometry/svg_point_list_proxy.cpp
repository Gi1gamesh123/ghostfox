// The SVGPointList indexed getter/setter proxy. Reserved slots: 0 the original
// node, 1 animated flag, 2 the native point-identity cache.
#include "web_geometry/svg_points.h"
#include "js/PropertyDescriptor.h"
#include "js/String.h"
#include "jsfriendapi.h"

namespace ruyijs::web_geometry {
const JSClass kSvgPointListClass =
    PROXY_CLASS_DEF("SVGPointList", JSCLASS_HAS_RESERVED_SLOTS(3) | JSCLASS_FOREGROUND_FINALIZE);
namespace {
char pointListFamily;
bool Index(JSContext* cx, JS::HandleId id, uint32_t* index, bool* indexed) {
  *indexed = id.isInt();
  if (*indexed) { *index = uint32_t(id.toInt()); return true; }
  if (!id.isString()) return true;
  JSLinearString* text = JS_EnsureLinearString(cx, id.toString());
  if (!text) return false;
  *indexed = js::StringIsArrayIndex(text, index) && *index != UINT32_MAX;
  return true;
}
JSObject* Expando(JS::HandleObject proxy) { return &js::GetProxyPrivate(proxy).toObject(); }
class Handler final : public js::BaseProxyHandler {
 public:
  constexpr Handler() : BaseProxyHandler(&pointListFamily) {}
  bool getOwnPropertyDescriptor(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
      JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> desc) const override {
    uint32_t index = 0; bool indexed;
    if (!Index(cx, id, &index, &indexed)) return false;
    if (indexed) {
      desc.reset();
      JS::RootedValue point(cx);
      if (!SvgPointListItem(cx, proxy, index, &point)) return false;
      if (!point.isUndefined())
        desc.set(mozilla::Some(JS::PropertyDescriptor::Data(point,
            {JS::PropertyAttribute::Enumerable, JS::PropertyAttribute::Configurable,
             JS::PropertyAttribute::Writable})));
      return true;
    }
    JS::RootedObject expando(cx, Expando(proxy));
    return JS_GetOwnPropertyDescriptorById(cx, expando, id, desc);
  }
  bool defineProperty(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
      JS::Handle<JS::PropertyDescriptor> desc, JS::ObjectOpResult& result) const override {
    uint32_t index = 0; bool indexed;
    if (!Index(cx, id, &index, &indexed)) return false;
    if (indexed) {
      if (!desc.isDataDescriptor()) return result.failNotDataDescriptor();
      JS::RootedValue value(cx, desc.hasValue() ? desc.value() : JS::UndefinedValue());
      return SvgPointIndexedSet(cx, proxy, index, value) && result.succeed();
    }
    JS::RootedObject expando(cx, Expando(proxy));
    return JS_DefinePropertyById(cx, expando, id, desc, result);
  }
  bool ownPropertyKeys(JSContext* cx, JS::HandleObject proxy, JS::MutableHandleIdVector keys) const override {
    uint32_t length = 0;
    if (!SvgPointListLength(cx, proxy, &length)) return false;
    for (uint32_t i = 0; i < length; ++i)
      if (!keys.append(JS::PropertyKey::Int(i))) { JS_ReportOutOfMemory(cx); return false; }
    JS::RootedObject expando(cx, Expando(proxy));
    JS::RootedIdVector extra(cx);
    if (!js::GetPropertyKeys(cx, expando, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &extra)) return false;
    if (!keys.appendAll(extra)) { JS_ReportOutOfMemory(cx); return false; }
    return true;
  }
  bool delete_(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, JS::ObjectOpResult& result) const override {
    uint32_t index = 0, length = 0; bool indexed;
    if (!Index(cx, id, &index, &indexed)) return false;
    if (indexed) {
      if (!SvgPointListLength(cx, proxy, &length)) return false;
      return index >= length ? result.succeed() : result.failCantDelete();
    }
    JS::RootedObject expando(cx, Expando(proxy));
    return JS_DeletePropertyById(cx, expando, id, result);
  }
  bool set(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, JS::HandleValue value,
           JS::HandleValue receiver, JS::ObjectOpResult& result) const override {
    uint32_t index = 0; bool indexed;
    if (!Index(cx, id, &index, &indexed)) return false;
    if (indexed) return SvgPointIndexedSet(cx, proxy, index, value) && result.succeed();
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
    *extensible = true;
    return true;
  }
  const char* className(JSContext*, JS::HandleObject) const override { return "SVGPointList"; }
  void finalize(JS::GCContext*, JSObject* proxy) const override {
    const JS::Value& slot = js::GetProxyReservedSlot(proxy, 2);
    if (!slot.isUndefined()) delete static_cast<SvgPointListNative*>(slot.toPrivate());
  }
};
const Handler handler;
}  // namespace
const js::BaseProxyHandler* SvgPointListHandler() { return &handler; }
}  // namespace ruyijs::web_geometry
