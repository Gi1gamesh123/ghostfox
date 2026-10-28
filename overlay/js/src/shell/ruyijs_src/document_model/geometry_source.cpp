#include "document_model/state.h"
#include "document_model/geometry_source.h"
#include "domain/document_model/foreign_element_interfaces.h"
#include "js/Realm.h"
#include <cstring>

namespace ruyijs::document_model {
JSObject* Realm::svgPrototype(bool text) {
  const auto* iface = state_->Find(text ? "SVGTextContentElement" : "SVGGraphicsElement");
  return iface ? iface->prototype.get() : nullptr;
}
bool Realm::ReadGeometryNode(JS::HandleValue value, GeometryNode* out,
    JS::MutableHandleObject parent, JS::MutableHandleObjectVector children) {
  NodeSource source;
  if (!ResolveNodeSource(value, &source)) return false;
  State& owner = *source.owner->state_;
  GeometryNode result;
  std::vector<NodeKey> keys;
  NodeKey parentKey;
  {
    JSAutoRealm realm(owner.cx, owner.page);
    NodeSnapshot node;
    if (!owner.Check(owner.model.Inspect(source.key, &node))) return false;
    result.element = node.kind == NodeKind::Element;
    result.connected = node.is_connected;
    result.localName = node.local_name.value_or(u"");
    result.factory = source.factory->geometryFactory_;
    parentKey = node.parent_element;
    const char* name = node.interface_name;
    for (unsigned depth = 0; depth < 8; ++depth) {
      result.graphics |= std::strcmp(name, "SVGGraphicsElement") == 0;
      result.textContent |= std::strcmp(name, "SVGTextContentElement") == 0;
      const char* next = nullptr;
      for (const auto& entry : ruyijs::domain::document_model::kSvgElementInterfaces)
        if (std::strcmp(entry.name, name) == 0) { next = entry.parent; break; }
      if (!next) break;
      name = next;
    }
    if (result.element && !owner.Check(owner.model.Attributes(source.key, &result.attributes))) return false;
    if (result.textContent) {
      std::optional<DomString> text;
      if (!owner.Check(owner.model.TextContent(source.key, &text))) return false;
      result.text = text.value_or(u"");
    }
    if (!owner.Check(owner.model.Children(source.key, &keys))) return false;
  }
  parent.set(nullptr);
  if (parentKey.node && !source.owner->WrapNode(parentKey, parent)) return false;
  children.clear();
  for (auto key : keys) {
    NodeSnapshot node;
    if (!owner.Check(owner.model.Inspect(key, &node))) return false;
    if (node.kind != NodeKind::Element) continue;
    JS::RootedObject child(owner.cx);
    if (!source.owner->WrapNode(key, &child) || !children.append(child)) return false;
  }
  *out = std::move(result);
  return true;
}
}
