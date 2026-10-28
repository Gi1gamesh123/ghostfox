#include "web_geometry/rect_list.h"
#include "js/Realm.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
#include "js/Array.h"

namespace ruyijs::web_geometry {
namespace {
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  bool item = WebKit::Slot(args) != 0;
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!object || JS::GetClass(object) != &kRectListClass)
    return WebKit::Fail(cx, "DOMRectList member called on an incompatible receiver");
  if (!item) { args.rval().setNumber(RectListLength(cx,object)); return true; }
  if (!argc) return WebKit::Fail(cx, "DOMRectList.item requires an index");
  uint32_t index;
  if (!JS::ToUint32(cx, args[0], &index)) return false;
  if (index >= RectListLength(cx,object)) { args.rval().setNull(); return true; }
  return RectListItem(cx,object,index,args.rval());
}
}
bool NewRectList(State& s, JS::MutableHandleValue result) {
  double zero[4] = {};
  return NewRectList(s,zero,true,result);
}
uint32_t RectListLength(JSContext* cx, JS::HandleObject list) {
  JSAutoRealm realm(cx,list);
  JS::RootedObject array(cx,&js::GetProxyReservedSlot(list,0).toObject());
  uint32_t length=0;
  MOZ_ALWAYS_TRUE(JS::GetArrayLength(cx,array,&length));
  return length;
}
bool RectListItem(JSContext* cx, JS::HandleObject list, uint32_t index, JS::MutableHandleValue result) {
  {
    JSAutoRealm realm(cx,list);
    JS::RootedObject array(cx,&js::GetProxyReservedSlot(list,0).toObject());
    if (!JS_GetElement(cx,array,index,result)) return false;
  }
  return JS_WrapValue(cx,result);
}
bool NewRectList(State& s, const double* rect, bool present, JS::MutableHandleValue result) {
  std::vector<std::array<double,4>> rects;
  if (present) rects.push_back({rect[0], rect[1], rect[2], rect[3]});
  return NewRectList(s, rects, result);
}
bool NewRectList(State& s, const std::vector<std::array<double,4>>& rects, JS::MutableHandleValue result) {
  JS::RootedObject array(s.cx,JS::NewArrayObject(s.cx,rects.size()));
  if (!array) return false;
  for (size_t i = 0; i < rects.size(); ++i) {
    JS::RootedValue item(s.cx);
    if (!NewValue(s,Rect,rects[i].data(),nullptr,&item) || !JS_SetElement(s.cx,array,uint32_t(i),item)) return false;
  }
  JS::RootedObject expando(s.cx, JS_NewObjectWithGivenProto(s.cx, nullptr, nullptr));
  if (!expando) return false;
  JS::RootedValue value(s.cx, JS::ObjectValue(*expando));
  js::ProxyOptions options;
  options.setClass(&kRectListClass);
  JS::RootedObject list(s.cx, js::NewProxyObject(s.cx, RectListHandler(), value, s.rectList.prototype, options));
  if (!list) return false;
  js::SetProxyReservedSlot(list, 0, JS::ObjectValue(*array));
  result.setObject(*list);
  return true;
}
bool Realm::NewElementRects(const std::array<double,4>& rect, bool present, bool list,
                            JS::MutableHandleValue result) {
  State& s=*state_;
  {
    JSAutoRealm realm(s.cx,s.kit.page);
    if (list) {
      if (!NewRectList(s,rect.data(),present,result)) return false;
    } else if (!NewValue(s,Rect,rect.data(),nullptr,result)) return false;
  }
  return JS_WrapValue(s.cx,result);
}
bool InstallRectList(State& s) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define("DOMRectList", &kRectListClass, plain, WebKit::Reject, &s.rectList) ||
      !s.kit.Accessor(s.rectList, "DOMRectList", "length", Member, nullptr, 0) ||
      !s.kit.Method(s.rectList, "DOMRectList", "item", Member, 1, 1)) return false;
  JS::RootedObject array(s.cx, JS::GetRealmArrayPrototype(s.cx));
  JS::RootedValue values(s.cx);
  JS::RootedId id(s.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(s.cx, JS::SymbolCode::iterator)));
  return array && JS_GetProperty(s.cx, array, "values", &values) &&
         JS_DefinePropertyById(s.cx, s.rectList.prototype, id, values, 0);
}
}
