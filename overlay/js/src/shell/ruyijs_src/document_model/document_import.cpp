#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
bool DocumentImport(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto* target = DocumentReceiver(cx, object, &args);
  if (!target) return false;
  if (!argc) return TypeError(cx, "Document.importNode requires one argument");
  auto* source = NodeReceiver(cx, args[0]);
  if (!source) return false;
  State& owner = *target->owner;
  owner.Observe("Document", "importNode", int(ApiAccess::Call));

  // Firefox's BooleanOrImportNodeOptions union uses a dictionary for objects
  // and null; undefined uses the optional argument's default boolean false.
  // The disabled customElementRegistry member is deliberately not read.
  JS::RootedValue options(cx, args.get(1));
  bool deep = JS::ToBoolean(options);
  if (options.isNull()) deep = true;
  if (options.isObject()) {
    JS::RootedObject dictionary(cx, &options.toObject());
    JS::RootedValue self_only(cx);
    if (!JS_GetProperty(cx, dictionary, "selfOnly", &self_only)) return false;
    deep = !JS::ToBoolean(self_only);
  }
  // The getter may mutate or adopt the source. Read its current owner/key only
  // after conversion; args keeps the original wrapper alive throughout.
  Draft draft;
  return owner.Check(owner.model.ImportNode(target->key, source->owner->model,
                                            source->key, deep, &draft)) &&
      owner.Commit(draft, args.rval());
}
}
