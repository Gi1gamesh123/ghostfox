#include "document_model/state.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
constexpr const char* fields[] = {"shadowRoot", "assignedSlot", "slot"};
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& function = *FunctionState(args);
  const int field = FunctionSlot(args);
  auto* entry = ElementReceiver(cx, args.thisv());
  if (!entry) return false;
  function.Observe("Element", fields[field], int(ApiAccess::Read));
  JS::RootedValue result(cx, JS::NullValue());
  {
    State& owner = *entry->owner;
    JSAutoRealm realm(cx, owner.page);
    if (field == 2) {
      std::optional<DomString> value;
      if (!owner.Check(owner.model.GetAttribute(entry->key, u"slot", &value)) ||
          !Value(cx, value ? DomStringView(*value) : DomStringView(), &result)) return false;
    } else if (field == 0) {
      NodeSnapshot node;
      if (!owner.Check(owner.model.Inspect(entry->key, &node))) return false;
      if (node.shadow_root.node) {
        ShadowSnapshot root;
        if (!owner.Check(owner.model.InspectShadow(node.shadow_root, &root))) return false;
        if (!root.options.closed && !owner.Wrap(node.shadow_root, &result)) return false;
      }
    }
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Write(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& function = *FunctionState(args);
  auto* entry = ElementReceiver(cx, args.thisv());
  if (!entry) return false;
  function.Observe("Element", "slot", int(ApiAccess::Write));
  if (!argc) return TypeError(cx, "Element slot setter requires a value");
  DomString value;
  // Preserve standard string coercion, but propagate exceptions where the
  // public implementation crashes. No failed conversion writes the attribute.
  if (!String(cx, args[0], &value)) return false;
  State& owner = *entry->owner;
  {
    JSAutoRealm realm(cx, owner.page);
    if (!owner.Check(owner.model.SetAttribute(entry->key, u"slot", value))) return false;
  }
  args.rval().setUndefined(); return true;
}
bool Attach(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& function = *FunctionState(args);
  auto* entry = ElementReceiver(cx, args.thisv());
  if (!entry) return false;
  function.Observe("Element", "attachShadow", int(ApiAccess::Call));
  if (!argc) return TypeError(cx, "attachShadow requires an argument");
  ShadowOptions options;
  if (!ReadShadowOptions(cx, args[0], &options)) return false;
  // Conversion may adopt the host. Resolve its current owner after getters.
  entry = ElementReceiver(cx, args.thisv());
  if (!entry) return false;
  State& owner = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, owner.page);
    Draft draft;
    if (!owner.Check(owner.model.AttachShadow(entry->key, options, &draft)) ||
        !owner.Commit(draft, &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
}
bool InstallShadowDOM(State& state) {
  for (int i = 0; i < int(std::size(fields)); ++i)
    if (!state.Accessor("Element", fields[i], Read, i, i == 2 ? Write : nullptr)) return false;
  return state.Method("Element", "attachShadow", Attach, 1, 0) && InstallShadowRootMembers(state);
}
}
