#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
Wrapper* TextReceiver(JSContext* cx, const JS::CallArgs& args) {
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto* entry = object && JS::GetClass(object) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  if (!entry || entry->dead || !entry->owner) {
    TypeError(cx, "Text receiver is incompatible"); return nullptr;
  }
  JSAutoRealm realm(cx, entry->owner->page);
  NodeSnapshot value;
  if (!entry->owner->Check(entry->owner->model.Inspect(entry->key, &value))) return nullptr;
  if (value.kind != NodeKind::Text) { TypeError(cx, "Expected a Text receiver"); return nullptr; }
  return entry;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int field = FunctionSlot(args);
  auto* entry = TextReceiver(cx, args);
  if (!entry) return false;
  if (!field) { args.rval().setNull(); return true; }
  DomString value;
  {
    JSAutoRealm realm(cx, entry->owner->page);
    if (!entry->owner->Check(entry->owner->model.WholeText(entry->key, &value))) return false;
  }
  return Value(cx, DomStringView(value), args.rval());
}
bool Split(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* entry = TextReceiver(cx, args);
  if (!entry) return false;
  if (!argc) return TypeError(cx, "splitText requires an offset");
  uint32_t offset;
  if (!JS::ToUint32(cx, args[0], &offset)) return false;
  State& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    NodeSnapshot value;
    if (!state.Check(state.model.Inspect(entry->key, &value))) return false;
    if (offset > value.data_length) return ThrowDOM(state, "IndexSizeError", "Text offset exceeds length");
    Draft draft;
    if (!state.Check(state.model.Create(value.owner_document, {NodeKind::Text, {}, {}}, &draft)) ||
        !state.Commit(draft, &result)) return false;
    if (!state.Check(state.model.SplitTextInto(entry->key, draft.root, offset))) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
}
bool InstallTextMembers(State& state) {
  return state.Accessor("Text", "assignedSlot", Get, 0) &&
      state.Accessor("Text", "wholeText", Get, 1) &&
      state.Method("Text", "splitText", Split, 1, 0);
}
}
