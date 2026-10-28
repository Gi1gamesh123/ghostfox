#include "document_model/html_control.h"
#include "js/Object.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
bool Content(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLTemplateElement");
  if (!entry) return false;
  auto& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    JS::RootedObject host(cx, entry->object);
    state.Observe("HTMLTemplateElement", "content", int(ApiAccess::Read));
    NodeKey fragment;
    if (!state.Check(state.model.TemplateContent(entry->key, &fragment))) return false;
    if (fragment.node) {
      if (!state.Wrap(fragment, &result)) return false;
    } else {
      NodeSnapshot node; Draft draft; NodeKey contentDocument;
      if (!state.Check(state.model.Inspect(entry->key, &node)) ||
          !state.Check(state.model.TemplateDocument(node.owner_document, &contentDocument)) ||
          !state.Check(state.model.CreateFragment(contentDocument, &draft)) ||
          !state.Commit(draft, &result, nullptr, entry->factory) ||
          !state.Check(state.model.BindTemplateContent(entry->key, draft.root))) return false;
    }
    JSAutoRealm hostRealm(cx, host);
    if (!JS_WrapValue(cx, &result)) return false;
    JS::SetReservedSlot(host, 4, result);
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
}
bool InstallTemplateElement(State& state) {
  return state.Accessor("HTMLTemplateElement", "content", Content, 0);
}
}
