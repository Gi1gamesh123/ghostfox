// Original HTMLElement native brand, shared by reflection and presentation.
#include "document_model/state.h"
#include "document_model/html_element_internal.h"
#include "runtime/web_kit.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
Wrapper* HTMLReceiver(JSContext* cx, JS::HandleValue value, State& function, const JS::CallArgs* args) {
  // Brand errors belong to the borrowed function's realm; data belongs to the receiver.
  JSAutoRealm functionRealm(cx, function.page);
  JS::RootedObject object(cx, value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  auto* entry = object && JS::GetClass(object) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  if (entry && entry->owner && !entry->dead) {
    State& owner = *entry->owner;
    NodeSnapshot node;
    {
      JSAutoRealm realm(cx, owner.page);
      if (!owner.Check(owner.model.Inspect(entry->key, &node))) return nullptr;
    }
    if (node.kind == NodeKind::Element && node.is_html_element) return entry;
  }
  std::string text;
  if (args && WebKit::BrandText(cx, *args, "HTMLElement", &text)) TypeError(cx, text.c_str());
  else TypeError(cx, "HTMLElement receiver is incompatible");
  return nullptr;
}
}
