// SVG length and SVG interface receiver ports for WebGeometry
// (docs/owners/WEB_GEOMETRY.md, 2026-10-21). Values stay with the original
// Element; views are weak identities on the original node Wrapper.
#include "document_model/state.h"
#include "document_model/svg_view_cache.h"
#include "domain/document_model/foreign_element_interfaces.h"
#include "domain/document_model/svg_length_fields.h"
#include "js/HeapAPI.h"
#include "js/Object.h"
#include "js/Wrapper.h"
#include <cstring>
#include <iterator>
#include <new>
#include <string>
namespace ruyijs::document_model {
namespace {
using ::ruyijs::domain::document_model::kSvgLengthFields;
// The node's interface or one of its SVG ancestors is `iface`.
bool Implements(const char* name, const char* iface) {
  for (unsigned depth = 0; name && depth < 8; ++depth) {
    if (std::strcmp(name, iface) == 0) return true;
    const char* next = nullptr;
    for (const auto& entry : ::ruyijs::domain::document_model::kSvgElementInterfaces)
      if (std::strcmp(entry.name, name) == 0) { next = entry.parent; break; }
    name = next;
  }
  return false;
}
}  // namespace
bool Realm::CheckSvgInterface(JS::HandleValue value, const char* iface, const char* member) {
  JSContext* cx = state_->cx;
  JS::RootedObject object(cx, value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (object && JS::GetClass(object) == &kNodeClass) {
    auto* wrapper = static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
    if (wrapper && wrapper->owner && !wrapper->dead) {
      NodeSnapshot node;
      if (!state_->Check(wrapper->owner->model.Inspect(wrapper->key, &node))) return false;
      if (Implements(node.interface_name, iface)) return true;
    }
  }
  const std::string error = std::string("'") + member + "' called on an object that does not implement interface " +
                            iface + ".";
  return TypeError(cx, error.c_str());
}
JSObject* Realm::svgLengthPrototype(unsigned field) {
  if (field >= std::size(kSvgLengthFields)) return nullptr;
  const auto* face = state_->Find(kSvgLengthFields[field].interfaceName);
  return face ? face->prototype.get() : nullptr;
}
bool Realm::CheckSvgLengthReceiver(JS::HandleValue value, unsigned field) {
  if (field >= std::size(kSvgLengthFields)) return TypeError(state_->cx, "Invalid SVG length field");
  const auto& info = kSvgLengthFields[field];
  return CheckSvgInterface(value, info.interfaceName, (std::string("get ") + info.member).c_str());
}
bool Realm::ReadSvgLength(JS::HandleValue value, unsigned field, SvgLengthValue* out, void** factory) {
  NodeSource source;
  if (!ResolveNodeSource(value, &source)) return false;
  State& owner = *source.owner->state_;
  bool supported = false;
  if (!owner.Check(owner.model.ReadSvgLength(source.key, field, &supported, out))) return false;
  if (!supported) return TypeError(state_->cx, "Node has no SVG length association");
  *factory = source.factory->geometryFactory_;
  return true;
}
bool Realm::WriteSvgLength(JS::HandleValue value, unsigned field, const SvgLengthValue& length) {
  NodeSource source;
  if (!ResolveNodeSource(value, &source)) return false;
  State& owner = *source.owner->state_;
  return owner.Check(owner.model.WriteSvgLength(source.key, field, length));
}
// The document element of the node's document, for root-relative units; null when absent.
bool Realm::SvgLengthRoot(JS::HandleValue value, JS::MutableHandleObject out) {
  NodeSource source;
  if (!ResolveNodeSource(value, &source)) return false;
  State& owner = *source.owner->state_;
  NodeSnapshot node;
  DocumentSnapshot document;
  out.set(nullptr);
  if (!owner.Check(owner.model.Inspect(source.key, &node))) return false;
  const NodeKey documentKey = node.owner_document.node ? node.owner_document : source.key;
  if (!owner.Check(owner.model.InspectDocument(documentKey, &document))) return false;
  if (!document.document_element.node) return true;
  return source.owner->WrapNode(document.document_element, out) && JS_WrapObject(state_->cx, out);
}
bool Realm::SvgLengthView(JS::HandleValue value, unsigned field, unsigned role, JS::HandleObject replacement,
                          JS::MutableHandleObject out) {
  JSContext* cx = state_->cx;
  Wrapper* node = NodeReceiver(cx, value);
  if (!node) return false;
  if (!node->factory || role >= 3 || field >= std::size(kSvgLengthFields))
    return TypeError(cx, "Invalid SVG length association");
  if (!node->svgViews) {
    if (!replacement) { out.set(nullptr); return true; }
    auto cache = std::unique_ptr<SvgViewCache>(new(std::nothrow) SvgViewCache(JS::GetObjectZone(node->object)));
    if (!cache) { JS_ReportOutOfMemory(cx); return false; }
    node->svgViews = std::move(cache);
  }
  auto& lengths = node->svgViews->entries.get().lengths;
  const unsigned slot = field * 3 + role;
  if (replacement) {
    JS::RootedObject native(cx, js::CheckedUnwrapStatic(replacement));
    if (!native || JS::GetObjectZone(native) != JS::GetObjectZone(node->object))
      return TypeError(cx, "SVG length factory zone mismatch");
    lengths[slot] = native;  // infallible allocation in this no-exception adapter
  }
  const auto found = lengths.find(slot);
  JS::RootedObject hit(cx, found == lengths.end() ? nullptr : found->second.get());
  if (hit) JS::ExposeObjectToActiveJS(hit);
  out.set(hit);
  return !hit || JS_WrapObject(cx, out);
}
}  // namespace ruyijs::document_model
