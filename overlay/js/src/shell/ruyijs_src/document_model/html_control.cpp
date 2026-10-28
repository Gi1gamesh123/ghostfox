#include "document_model/html_control.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cstring>
#include <string>

namespace ruyijs::document_model {
Wrapper* ControlReceiver(JSContext* cx, JS::HandleValue value, const char* name,
                         const char* member) {
  auto incompatible = [&]() -> Wrapper* {
    if (member && name)
      TypeError(cx, ("'" + std::string(member) +
                     "' called on an object that does not implement interface " + name + ".").c_str());
    else
      TypeError(cx, "Control member called on incompatible receiver");
    return nullptr;
  };
  JS::RootedObject object(cx, value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  auto* entry = object && JS::GetClass(object) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  if (!entry || entry->dead || !entry->owner) return incompatible();
  JSAutoRealm realm(cx, entry->owner->page);
  NodeSnapshot node;
  if (!entry->owner->Check(entry->owner->model.Inspect(entry->key, &node))) return nullptr;
  bool media = name && std::strcmp(name, "HTMLMediaElement") == 0 &&
      (!std::strcmp(node.interface_name, "HTMLAudioElement") ||
       !std::strcmp(node.interface_name, "HTMLVideoElement"));
  if (node.kind != NodeKind::Element || (name && std::strcmp(node.interface_name, name) && !media))
    return incompatible();
  return entry;
}
bool IsMediaElementValue(JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  if (!object || JS::GetClass(object) != &kNodeClass) return false;
  auto* entry = static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
  if (!entry || entry->dead || !entry->owner) return false;
  NodeSnapshot node;
  if (entry->owner->model.Inspect(entry->key, &node).code != ErrorCode::None) return false;
  return node.kind == NodeKind::Element &&
         (!std::strcmp(node.interface_name, "HTMLAudioElement") ||
          !std::strcmp(node.interface_name, "HTMLVideoElement"));
}
}
