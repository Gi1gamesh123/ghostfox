// ProcessingInstruction members (Firefox 158 ProcessingInstruction.webidl):
// `target`, and LinkStyle's `sheet`. Nodes come from the one Document Model
// factory (Document.createProcessingInstruction, the XML parser, cloning); the
// target is the domain node's name. `sheet` is null: no xml-stylesheet
// processing instruction applies a style sheet in this runtime (D147).
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <string>

namespace ruyijs::document_model {
namespace {
const char* const kMembers[] = {"target", "sheet"};

// The node behind a ProcessingInstruction receiver, or Firefox's brand error.
Wrapper* Receiver(JSContext* cx, const JS::CallArgs& args, int member, NodeSnapshot* node) {
  JSObject* object = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  auto* entry = object && JS::GetClass(object) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  if (entry && entry->owner && !entry->dead) {
    State& owner = *entry->owner;
    JSAutoRealm realm(cx, owner.page);
    if (!owner.Check(owner.model.Inspect(entry->key, node))) return nullptr;
    if (node->kind == NodeKind::ProcessingInstruction) return entry;
  }
  TypeError(cx, (std::string("'get ") + kMembers[member] +
                 "' called on an object that does not implement interface ProcessingInstruction.").c_str());
  return nullptr;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int member = FunctionSlot(args);
  NodeSnapshot node;
  Wrapper* entry = Receiver(cx, args, member, &node);
  if (!entry) return false;
  entry->owner->Observe("ProcessingInstruction", kMembers[member], int(ApiAccess::Read));
  if (member == 1) { args.rval().setNull(); return true; }
  return Value(cx, DomStringView(node.node_name), args.rval());
}
}  // namespace

bool InstallProcessingInstruction(State& state) {
  return state.Accessor("ProcessingInstruction", "target", Get, 0) &&
         state.Accessor("ProcessingInstruction", "sheet", Get, 1);
}
}  // namespace ruyijs::document_model
