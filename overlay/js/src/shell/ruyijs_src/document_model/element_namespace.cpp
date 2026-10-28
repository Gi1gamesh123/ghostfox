#include "document_model/state.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
bool CreateElementNS(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx, args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto* document = DocumentReceiver(cx, object);
  if (!document) return false;
  if (argc < 2) return TypeError(cx, "Document.createElementNS requires two arguments");
  DomString namespace_uri, name;
  std::optional<DomString> is;
  if (!String(cx, args[0], &namespace_uri, true) || !String(cx, args[1], &name)) return false;
  if (args.get(2).isObject()) {
    JS::RootedObject options(cx, &args[2].toObject());
    JS::RootedValue custom(cx);
    if (!JS_GetProperty(cx, options, "is", &custom)) return false;
    if (!custom.isUndefined()) {
      DomString converted;
      if (!String(cx, custom, &converted)) return false;
      is = std::move(converted);
    }
  } else if (!args.get(2).isNullOrUndefined()) {
    DomString converted;
    if (!String(cx, args[2], &converted)) return false;
  }
  State& owner = *document->owner;
  State& function = *FunctionState(args);
  function.Observe("Document", "createElementNS", int(ApiAccess::Call));
  Draft draft;
  return function.Check(owner.model.CreateElementNS(document->key, namespace_uri, name, is, &draft)) &&
      owner.Commit(draft, args.rval(), nullptr, document->factory);
}
}
