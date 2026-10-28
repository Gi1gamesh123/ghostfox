// Internal views resolve the original Wrapper owner on every operation.
#include "document_model/state.h"
#include "document_model/svg_view_cache.h"
#include "js/HeapAPI.h"
#include "js/Wrapper.h"
#include <new>
#include <cstring>
#include "js/Object.h"
namespace ruyijs::document_model {
JSObject* Realm::svgViewBoxPrototype(unsigned index) {
  static const char* names[] = {"SVGSVGElement", "SVGSymbolElement", "SVGMarkerElement",
                                 "SVGPatternElement", "SVGViewElement"};
  if (index >= 5) return nullptr;
  const auto* face = state_->Find(names[index]);
  return face ? face->prototype.get() : nullptr;
}
bool Realm::CheckSvgViewBoxReceiver(JS::HandleValue value, unsigned index) {
  static const char* names[] = {"SVGSVGElement", "SVGSymbolElement", "SVGMarkerElement",
                                 "SVGPatternElement", "SVGViewElement"};
  JSContext* cx = state_->cx;
  JS::RootedObject object(cx, value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (index < 5 && object && JS::GetClass(object) == &kNodeClass) {
    auto* wrapper = static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
    if (wrapper && wrapper->owner && !wrapper->dead) {
      NodeSnapshot node;
      if (!state_->Check(wrapper->owner->model.Inspect(wrapper->key, &node))) return false;
      if (std::strcmp(node.interface_name, names[index]) == 0) return true;
    }
  }
  static const char* errors[] = {
    "'get viewBox' called on an object that does not implement interface SVGSVGElement.",
    "'get viewBox' called on an object that does not implement interface SVGSymbolElement.",
    "'get viewBox' called on an object that does not implement interface SVGMarkerElement.",
    "'get viewBox' called on an object that does not implement interface SVGPatternElement.",
    "'get viewBox' called on an object that does not implement interface SVGViewElement."};
  return TypeError(cx, index < 5 ? errors[index] : "Invalid SVG viewBox interface index");
}
bool Realm::ReadSvgViewBox(JS::HandleValue value, bool* supported, SvgViewBoxValue* out, void** factory) {
  NodeSource source;
  if (!ResolveNodeSource(value, &source)) return false;
  State& owner = *source.owner->state_;
  if (!owner.Check(owner.model.ReadSvgViewBox(source.key, supported, out))) return false;
  *factory = source.factory->geometryFactory_;
  return true;
}
bool Realm::WriteSvgViewBoxField(JS::HandleValue value, unsigned field, float number) {
  NodeSource source;
  if (!ResolveNodeSource(value, &source)) return false;
  State& owner = *source.owner->state_;
  return owner.Check(owner.model.WriteSvgViewBoxField(source.key, field, number));
}
bool Realm::SvgViewBoxView(JS::HandleValue value, unsigned role, JS::HandleObject replacement,
                           JS::MutableHandleObject out) {
  JSContext* cx = state_->cx;
  Wrapper* node = NodeReceiver(cx, value);
  if (!node) return false;
  if (!node->factory || role >= 3) return TypeError(cx, "Invalid SVG viewBox association");
  bool supported = false; SvgViewBoxValue ignored;
  if (!node->owner->Check(node->owner->model.ReadSvgViewBox(node->key, &supported, &ignored))) return false;
  if (!supported) return TypeError(cx, "Node has no SVG viewBox association");
  if (!node->svgViews) {
    if (!replacement) { out.set(nullptr); return true; }
    auto cache = std::unique_ptr<SvgViewCache>(new(std::nothrow) SvgViewCache(JS::GetObjectZone(node->object)));
    if (!cache) { JS_ReportOutOfMemory(cx); return false; }
    node->svgViews = std::move(cache);
  }
  auto& entry = node->svgViews->entries.get().views[role];
  if (replacement) {
    JS::RootedObject native(cx, js::CheckedUnwrapStatic(replacement));
    if (!native || JS::GetObjectZone(native) != JS::GetObjectZone(node->object))
      return TypeError(cx, "SVG viewBox factory zone mismatch");
    entry = native;
  }
  JS::RootedObject hit(cx, entry.get());
  if (hit) JS::ExposeObjectToActiveJS(hit);
  out.set(hit);
  return !hit || JS_WrapObject(cx, out);
}
}
