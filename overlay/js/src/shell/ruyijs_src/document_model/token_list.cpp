#include "document_model/html_control.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
DomStringView Attribute(const CollectionRecord& record) {
  return record.argument.empty() ? DomStringView(u"class") : DomStringView(record.argument);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* record = CollectionReceiver(cx, args.thisv(), CollectionKind::TokenList);
  if (!record) return false;
  State* state = record->dataOwner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state->page);
    std::optional<DomString> value;
    if (!state->Check(state->model.GetAttribute(record->scope, Attribute(*record), &value)) ||
        !Value(cx, DomStringView(value.value_or(u"")), &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* record = CollectionReceiver(cx, args.thisv(), CollectionKind::TokenList);
  if (!record) return false;
  if (Attribute(*record) == u"part") return PartTokenSet(*FunctionState(args), *record, args);
  DomString value;
  if (Attribute(*record) == u"sandbox" ? !ControlString(cx, args.get(0), &value)
                                      : !String(cx, args.get(0), &value, true)) return false;
  {
    State* state = record->dataOwner;
    JSAutoRealm realm(cx, state->page);
    if (!state->Check(state->model.SetAttribute(record->scope, Attribute(*record), value))) return false;
  }
  args.rval().setUndefined(); return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* record = CollectionReceiver(cx, args.thisv(), CollectionKind::TokenList);
  if (!record) return false;
  const auto op = TokenOperation(FunctionSlot(args));
  if (Attribute(*record) == u"part") return PartTokenCall(*FunctionState(args), *record, args, op);
  const bool sandbox = Attribute(*record) == u"sandbox";
  if (sandbox && argc < (op == TokenOperation::Replace ? 2u : 1u)) return TypeError(cx, "DOMTokenList argument is required");
  if (op == TokenOperation::TokenItem) {
    uint32_t index = 0;
    if (!JS::ToUint32(cx, args.get(0), &index)) return false;
    if (index >= CollectionCount(*record)) { args.rval().setNull(); return true; }
    return CollectionIndex(*record->dataOwner, *record, index, args.rval());
  }
  if (op == TokenOperation::Supports) {
    DomString ignored;
    if (sandbox && !ControlString(cx, args.get(0), &ignored)) return false;
    args.rval().setBoolean(sandbox); return true;
  }
  auto convert = [&](JS::HandleValue v, DomString* out) {
    return sandbox ? ControlString(cx, v, out) : String(cx, v, out);
  };
  if (op == TokenOperation::Add || op == TokenOperation::Remove) {
    for (unsigned i = 0; i < argc; ++i) {
      DomString token; bool ignored;
      if (!convert(args[i], &token)) return false;
      State* state = record->dataOwner;
      JSAutoRealm realm(cx, state->page);
      if (!state->Check(state->model.EditTokens(record->scope, Attribute(*record),
          op == TokenOperation::Add ? TokenAction::Add : TokenAction::Remove, token, u"", false, false, &ignored))) return false;
    }
    args.rval().setUndefined(); return true;
  }
  DomString first, second;
  if (!convert(args.get(0), &first) || (op == TokenOperation::Replace && !convert(args.get(1), &second))) return false;
  const bool hasForce = argc > 1, force = hasForce && JS::ToBoolean(args[1]);
  bool result = false;
  const auto action = op == TokenOperation::Contains ? TokenAction::Contains : op == TokenOperation::Toggle ? TokenAction::Toggle : TokenAction::Replace;
  State* state = record->dataOwner;
  JSAutoRealm realm(cx, state->page);
  if (!state->Check(state->model.EditTokens(record->scope, Attribute(*record), action,
      first, second, force, hasForce, &result))) return false;
  args.rval().setBoolean(result); return true;
}
}
bool InstallTokenList(State& state) {
  struct Operation { const char* name; TokenOperation op; unsigned length; };
  static const Operation operations[] = {
    {"add", TokenOperation::Add, 0}, {"remove", TokenOperation::Remove, 0}, {"toggle", TokenOperation::Toggle, 1},
    {"contains", TokenOperation::Contains, 1}, {"replace", TokenOperation::Replace, 2}, {"supports", TokenOperation::Supports, 1}, {"item", TokenOperation::TokenItem, 1}
  };
  if (!state.Accessor("DOMTokenList", "length", CollectionLength, int32_t(CollectionKind::TokenList)) ||
      !state.Accessor("DOMTokenList", "value", Get, 0, Set) ||
      !state.Method("DOMTokenList", "toString", Get, 0, 0)) return false;
  for (const auto& operation : operations)
    if (!state.Method("DOMTokenList", operation.name, Call, operation.length, int32_t(operation.op))) return false;
  return state.Method("DOMTokenList", "forEach", CollectionForEach, 1, int32_t(CollectionKind::TokenList)) &&
      InstallCollectionMethods(state, "DOMTokenList");
}
}
