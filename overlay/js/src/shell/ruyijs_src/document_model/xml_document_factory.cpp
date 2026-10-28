#include "document_model/parsing.h"
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Wrapper.h"
namespace ruyijs::document_model {
bool CreateXMLDocument(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  JS::RootedObject service(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!service || JS::GetClass(service) != &kServiceClass ||
      JS::GetReservedSlot(service, 1).toInt32() != 2)
    return TypeError(cx, "DOMImplementation receiver is incompatible");
  if (argc < 2) return TypeError(cx, "createDocument requires two arguments");
  state.Observe("DOMImplementation", "createDocument", int(ApiAccess::Call));
  DomString namespace_uri, name;
  if (!args[0].isNullOrUndefined() && !PublicDocumentString(cx, args[0], &namespace_uri)) return false;
  if (!args[1].isNull() && !PublicDocumentString(cx, args[1], &name)) return false;
  NodeKey doctype;
  if (!args.get(2).isNullOrUndefined()) {
    if (!NodeArgument(state, args[2], &doctype)) return false;
    NodeSnapshot node;
    if (!state.Check(state.model.Inspect(doctype, &node))) return false;
    if (node.kind != NodeKind::DocumentType) return TypeError(cx, "Expected a DocumentType");
  }
  Draft draft;
  std::vector<std::pair<NodeKey, NodeKey>> rekey;
  Status status = state.model.CreatePublicDocument(namespace_uri, name, doctype, &draft, &rekey);
  state.Rekey(rekey);
  if (!state.Check(status)) return false;
  // The original transfer notification precedes wrapper rekeying. Once the
  // keys agree, root the moved wrapper from its new connected tree membership.
  state.SyncMembership(draft.root);
  return state.Commit(draft, args.rval());
}
}
