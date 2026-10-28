#include "document_model/state.h"
#include "document_model/svg_points_types.h"
#include "document_model/svg_view_cache.h"
#include "js/HeapAPI.h"
#include "js/Object.h"
#include "js/Wrapper.h"
#include <new>
namespace ruyijs::document_model {
JSObject* Realm::svgPointsPrototype(unsigned index) {
  static const char* names[] = {"SVGSVGElement", "SVGPolylineElement", "SVGPolygonElement"};
  const auto* face = index < 3 ? state_->Find(names[index]) : nullptr;
  return face ? face->prototype.get() : nullptr;
}
bool Realm::ReadSvgPoints(JS::HandleValue value, SvgPointsSnapshot* out, void** factory) {
  NodeSource source;
  if (!ResolveNodeSource(value, &source)) return false;
  State& owner = *source.owner->state_;
  if (!owner.Check(owner.model.ReadSvgPoints(source.key, out))) return false;
  *factory = source.factory->geometryFactory_; return true;
}
bool Realm::MutateSvgPoints(JS::HandleValue value, unsigned operation, uint32_t index,
                            const SvgPointRef& input, SvgPointRef* out) {
  NodeSource source;
  if (!ResolveNodeSource(value, &source)) return false;
  State& owner = *source.owner->state_;
  return owner.Check(owner.model.MutateSvgPoints(source.key, operation, index, input, out));
}
bool Realm::WriteSvgPointField(JS::HandleValue value, const SvgPointRef& point, unsigned field, float number) {
  NodeSource source;
  if (!ResolveNodeSource(value, &source)) return false;
  State& owner = *source.owner->state_;
  return owner.Check(owner.model.WriteSvgPointField(source.key, point, field, number));
}
bool Realm::SvgPointsView(JS::HandleValue value, unsigned role, JS::HandleObject replacement,
                           JS::MutableHandleObject out) {
  JSContext* cx = state_->cx;
  Wrapper* node = NodeReceiver(cx, value);
  if (!node) return false;
  if (!node->factory || role >= 2) return TypeError(cx, "Invalid SVG point list association");
  SvgPointsSnapshot source;
  if (!node->owner->Check(node->owner->model.ReadSvgPoints(node->key, &source))) return false;
  if (!source.supported) return TypeError(cx, "Node has no SVG point list association");
  if (!node->svgViews) {
    if (!replacement) { out.set(nullptr); return true; }
    auto cache = std::unique_ptr<SvgViewCache>(new(std::nothrow) SvgViewCache(JS::GetObjectZone(node->object)));
    if (!cache) { JS_ReportOutOfMemory(cx); return false; }
    node->svgViews = std::move(cache);
  }
  auto& entry = node->svgViews->entries.get().views[3 + role];
  if (replacement) {
    JS::RootedObject native(cx, js::CheckedUnwrapStatic(replacement));
    if (!native || JS::GetObjectZone(native) != JS::GetObjectZone(node->object))
      return TypeError(cx, "SVG point list factory zone mismatch");
    entry = native;
  }
  JS::RootedObject hit(cx, entry.get());
  if (hit) JS::ExposeObjectToActiveJS(hit);
  out.set(hit); return !hit || JS_WrapObject(cx, out);
}
}
