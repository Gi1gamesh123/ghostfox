#include "document_model/state.h"
#include "runtime/web_kit.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
// The caller roots the checked-unwrapped object throughout the native call.
Wrapper* DocumentReceiver(JSContext* cx, JS::HandleObject object, const JS::CallArgs* args) {
  if (object && JS::GetClass(object) == &kNodeClass) {
    auto* entry = static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
    if (entry && entry->owner && !entry->dead) {
      State& owner = *entry->owner;
      NodeSnapshot snapshot;
      JSAutoRealm realm(cx, owner.page);
      if (!owner.Check(owner.model.Inspect(entry->key, &snapshot))) return nullptr;
      if (snapshot.kind == NodeKind::Document) return entry;
    }
  }
  // Match Firefox's brand wording for Cloudflare's per-member fingerprint.
  std::string text;
  if (args && WebKit::BrandText(cx, *args, "Document", &text)) TypeError(cx, text.c_str());
  else TypeError(cx, "Document receiver is incompatible");
  return nullptr;
}
bool IsDocumentValue(JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  if (!object || JS::GetClass(object) != &kNodeClass) return false;
  auto* entry = static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
  if (!entry || entry->dead || !entry->owner) return false;
  NodeSnapshot node;
  return entry->owner->model.Inspect(entry->key, &node).code == ErrorCode::None && node.kind == NodeKind::Document;
}
bool ClearDocument(Wrapper& entry) {
  State& owner = *entry.owner;
  JSAutoRealm realm(owner.cx, owner.page);
  DocumentSnapshot snapshot;
  if (!owner.Check(owner.model.InspectDocument(entry.key, &snapshot))) return false;
  return Same(snapshot.document_element, {}) ||
      owner.Check(owner.model.SetTextContent(snapshot.document_element, {}));
}
}
