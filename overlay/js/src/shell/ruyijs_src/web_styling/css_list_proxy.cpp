#include "web_styling/cssom.h"
#include "js/PropertyDescriptor.h"
#include "js/String.h"
#include "jsfriendapi.h"

namespace ruyijs::web_styling {
const JSClass kRuleListClass = PROXY_CLASS_DEF("CSSRuleList", JSCLASS_HAS_RESERVED_SLOTS(2));
const JSClass kSheetListClass = PROXY_CLASS_DEF("StyleSheetList", JSCLASS_HAS_RESERVED_SLOTS(3));
const JSClass kMediaListClass = PROXY_CLASS_DEF("MediaList", JSCLASS_HAS_RESERVED_SLOTS(2));
const JSClass kKeyframesRuleClass = PROXY_CLASS_DEF("CSSKeyframesRule", JSCLASS_HAS_RESERVED_SLOTS(1));
namespace {
char listFamily;
bool Index(JSContext* cx, JS::HandleId id, uint32_t* index, bool* indexed) {
  *indexed = id.isInt();
  if (*indexed) { *index = uint32_t(id.toInt()); return true; }
  if (!id.isString()) return true;
  JSLinearString* text = JS_EnsureLinearString(cx, id.toString());
  if (!text) return false;
  *indexed = js::StringIsArrayIndex(text, index) && *index != UINT32_MAX;
  return true;
}
class Handler final : public js::NurseryAllocableProxyHandler {
 public:
  constexpr Handler() : NurseryAllocableProxyHandler(&listFamily) {}
  bool getOwnPropertyDescriptor(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
      JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> desc) const override {
    uint32_t index = 0; bool indexed;
    if (!Index(cx, id, &index, &indexed)) return false;
    if (indexed) {
      desc.reset();
      JS::RootedValue value(cx);
      if (!CssListAt(cx, proxy, index, &value)) return false;
      if (value.isUndefined()) return true;
      desc.set(mozilla::Some(JS::PropertyDescriptor::Data(value,
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
    if (indexed) return result.failCantRedefineProp();
    JS::RootedObject expando(cx, &js::GetProxyPrivate(proxy).toObject());
    return JS_DefinePropertyById(cx, expando, id, desc, result);
  }
  bool ownPropertyKeys(JSContext* cx, JS::HandleObject proxy,
                       JS::MutableHandleIdVector keys) const override {
    uint32_t length;
    if (!CssListLength(cx, proxy, &length)) return false;
    for (uint32_t i = 0; i < length; ++i) {
      JS::RootedId id(cx);
      if (!JS_IndexToId(cx, i, &id) || !keys.append(id)) return false;
    }
    JS::RootedObject expando(cx, &js::GetProxyPrivate(proxy).toObject());
    JS::RootedIdVector extra(cx);
    if (!js::GetPropertyKeys(cx, expando, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &extra))
      return false;
    if (!keys.appendAll(extra)) { JS_ReportOutOfMemory(cx); return false; }
    return true;
  }
  bool delete_(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
                JS::ObjectOpResult& result) const override {
    uint32_t index = 0; bool indexed;
    if (!Index(cx, id, &index, &indexed)) return false;
    if (indexed) {
      uint32_t length;
      if (!CssListLength(cx, proxy, &length)) return false;
      return index < length ? result.failCantDelete() : result.succeed();
    }
    JS::RootedObject expando(cx, &js::GetProxyPrivate(proxy).toObject());
    return JS_DeletePropertyById(cx, expando, id, result);
  }
  bool set(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
      JS::HandleValue value, JS::HandleValue receiver, JS::ObjectOpResult& result) const override {
    uint32_t index = 0; bool indexed;
    if (!Index(cx, id, &index, &indexed)) return false;
    if (indexed) return result.failReadOnly();
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
  const char* className(JSContext*, JS::HandleObject proxy) const override {
    return JS::GetClass(proxy)->name;
  }
};
const Handler handler;
}
const js::BaseProxyHandler* CssListHandler() { return &handler; }
}  // namespace ruyijs::web_styling
