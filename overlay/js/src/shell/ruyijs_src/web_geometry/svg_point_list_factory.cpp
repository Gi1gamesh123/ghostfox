#include "web_geometry/svg_points.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include "js/HeapAPI.h"
namespace ruyijs::web_geometry {
bool SvgPointListSnapshot(State& s, JS::HandleObject list, document_model::SvgPointsSnapshot* out) {
  JS::RootedValue node(s.cx, js::GetProxyReservedSlot(list, 0)); void* factory = nullptr;
  return JS_WrapValue(s.cx, &node) && s.svgPorts.pointsRead &&
      s.svgPorts.pointsRead(s.svgPorts.context, node, out, &factory);
}
bool SvgPointListLength(JSContext* cx, JS::HandleObject list, uint32_t* length) {
  document_model::SvgPointsSnapshot snapshot;
  if (!SvgPointListSnapshot(*SvgPointListData(list)->state, list, &snapshot)) return false;
  *length = uint32_t(snapshot.base.size()); return true;
}
bool SvgPointListItem(JSContext* cx, JS::HandleObject list, uint32_t index, JS::MutableHandleValue out) {
  auto& native = *SvgPointListData(list);
  document_model::SvgPointsSnapshot snapshot;
  if (!SvgPointListSnapshot(*native.state, list, &snapshot)) return false;
  const auto& values = js::GetProxyReservedSlot(list, 1).toBoolean() ? snapshot.animated : snapshot.base;
  if (index >= values.size()) { out.setUndefined(); return true; }
  {
    JSAutoRealm realm(cx, list);
    JS::RootedObject object(cx);
    auto& cache = native.cache.get().points;
    const auto found = cache.find(values[index].get());
    if (found != cache.end()) object = found->second;
    if (object) JS::ExposeObjectToActiveJS(object);
    else {
      if (!NewSvgPointObject(*native.state, values[index], list, &object)) return false;
      cache[values[index].get()] = object;
    }
    out.setObject(*object);
  }
  return JS_WrapValue(cx, out);
}
bool NewSvgPointList(State& s, JS::HandleValue node, unsigned role, JS::MutableHandleObject out) {
  JS::RootedObject expando(s.cx, JS_NewObjectWithGivenProto(s.cx, nullptr, nullptr));
  if (!expando) return false;
  JS::RootedValue value(s.cx, JS::ObjectValue(*expando));
  js::ProxyOptions options; options.setClass(&kSvgPointListClass);
  JS::RootedObject list(s.cx, js::NewProxyObject(s.cx, SvgPointListHandler(), value, s.svgPointList.prototype, options));
  if (!list) return false;
  auto native = std::unique_ptr<SvgPointListNative>(new(std::nothrow) SvgPointListNative(s, JS::GetObjectZone(list)));
  if (!native) { JS_ReportOutOfMemory(s.cx); return false; }
  js::SetProxyReservedSlot(list, 0, node);
  js::SetProxyReservedSlot(list, 1, JS::BooleanValue(role != 0));
  js::SetProxyReservedSlot(list, 2, JS::PrivateValue(native.release())); out.set(list); return true;
}
bool Realm::SvgPointsView(JS::HandleValue node, unsigned role, JS::MutableHandleValue out) {
  State& s = *state_;
  {
    JSAutoRealm realm(s.cx, s.kit.page);
    JS::RootedValue source(s.cx, node); JS::RootedObject object(s.cx), empty(s.cx);
    if (!JS_WrapValue(s.cx, &source) || !s.svgPorts.pointsView ||
        !s.svgPorts.pointsView(s.svgPorts.context, source, role, empty, &object)) return false;
    if (!object && (!NewSvgPointList(s, source, role, &object) ||
        !s.svgPorts.pointsView(s.svgPorts.context, source, role, object, &object))) return false;
    out.setObject(*object);
  }
  return JS_WrapValue(s.cx, out);
}
namespace {
bool Create(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args);
  if (!s.svgPorts.elementCheck || !s.svgPorts.elementCheck(s.svgPorts.context, args.thisv(),
      "SVGSVGElement", "createSVGPoint")) return false;
  SvgPointRef value;
  value = std::make_shared<domain::document_model::SvgPointValue>();
  JS::RootedObject object(cx), empty(cx);
  if (!NewSvgPointObject(s, value, empty, &object)) return false;
  args.rval().setObject(*object); return true;
}
bool GetList(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args);
  const unsigned slot = WebKit::Slot(args), role = slot % 2;
  const char* iface = slot < 2 ? "SVGPolylineElement" : "SVGPolygonElement";
  if (!s.svgPorts.elementCheck || !s.svgPorts.elementCheck(s.svgPorts.context, args.thisv(), iface,
      role ? "get animatedPoints" : "get points")) return false;
  document_model::SvgPointsSnapshot source; void* factory = nullptr;
  if (!s.svgPorts.pointsRead || !s.svgPorts.pointsRead(s.svgPorts.context, args.thisv(), &source, &factory)) return false;
  return factory && static_cast<Realm*>(factory)->SvgPointsView(args.thisv(), role, args.rval());
}
}
bool Realm::InstallSvgPointsProjection(JS::HandleObject prototype, unsigned index) {
  State& s = *state_; JSAutoRealm realm(s.cx, s.kit.page); WebFace face(s.cx); face.prototype = prototype;
  if (!index) return s.kit.Method(face, "SVGSVGElement", "createSVGPoint", Create, 0, 0);
  const char* name = index == 1 ? "SVGPolylineElement" : "SVGPolygonElement";
  return s.kit.Accessor(face, name, "points", GetList, nullptr, (index - 1) * 2) &&
      s.kit.Accessor(face, name, "animatedPoints", GetList, nullptr, (index - 1) * 2 + 1);
}
}
