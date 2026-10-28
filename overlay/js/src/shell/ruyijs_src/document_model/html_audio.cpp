// Audio's legacy factory creates the existing HTMLAudioElement node type.
// DocumentModel remains the sole source of node identity and reflected media
// attributes; this file only adapts the WebIDL factory into that owner.
#include "document_model/state.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
bool Audio(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  State& state = *FunctionState(args);
  Draft draft;
  if (!state.Check(state.model.Create(state.main, {NodeKind::Element, u"audio", {}}, &draft)))
    return false;
  if (!state.Commit(draft, args.rval())) return false;
  if (argc) {
    DomString src;
    if (!String(cx, args[0], &src) ||
        !state.Check(state.model.SetAttribute(draft.root, u"src", src))) return false;
  }
  return trace.Finish(true);
}
}  // namespace

bool InstallAudioElement(State& state) {
  JS::RootedObject factory(state.cx, state.Function(Audio, "Audio", 0, 0,
                                                     JSFUN_CONSTRUCTOR));
  JS::RootedObject prototype(state.cx, state.Find("HTMLAudioElement")->prototype);
  return factory && JS_DefineProperty(state.cx, factory, "prototype", prototype,
                                      JSPROP_READONLY | JSPROP_PERMANENT) &&
      JS_DefineProperty(state.cx, state.page, "Audio", factory, 0) &&
      state.helpers.Constructor("Audio", factory);
}
}  // namespace ruyijs::document_model
