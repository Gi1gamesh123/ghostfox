#include "document_model/parsing.h"
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
bool CreateDocumentType(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& factory = *FunctionState(args);
  JS::RootedObject service(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!service || JS::GetClass(service) != &kServiceClass ||
      JS::GetReservedSlot(service, 1).toInt32() != 2)
    return TypeError(cx, "DOMImplementation receiver is incompatible");
  if (argc < 3) return TypeError(cx, "createDocumentType requires three arguments");
  factory.Observe("DOMImplementation", "createDocumentType", int(ApiAccess::Call));
  DomString name, public_id, system_id;
  if (!PublicDocumentString(cx, args[0], &name) || !PublicDocumentString(cx, args[1], &public_id) || !PublicDocumentString(cx, args[2], &system_id)) return false;
  if (name.empty()) return StandardError(cx, "Failed to intern DocumentType name via native string interning");
  JS::RootedValue source(cx, JS::GetReservedSlot(service, 2));
  JS::RootedObject document(cx, source.isObject() ? js::CheckedUnwrapStatic(&source.toObject()) : nullptr);
  auto* entry = DocumentReceiver(cx, document, &args);
  if (!entry) return false;
  State& owner = *entry->owner;
  Draft draft;
  return owner.Check(owner.model.Create(entry->key, {NodeKind::DocumentType, name, {}, !public_id.empty() && !system_id.empty()}, &draft)) &&
      owner.Commit(draft, args.rval(), nullptr, &factory);
}
}
