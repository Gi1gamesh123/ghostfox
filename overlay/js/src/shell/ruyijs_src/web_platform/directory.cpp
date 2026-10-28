// Directory (dom/webidl/Directory.webidl, Exposed=(Window,Worker)). Its constructor is
// ChromeOnly and content only receives instances from a directory picker, which this
// runtime has not, so no instance is ever created; the interface object, members and
// receiver errors follow Firefox.
#include "web_platform/state.h"
#include "runtime/native_constructor_trace.h"
#include "jsapi.h"
#include "js/Object.h"

namespace ruyijs::web_platform {
namespace {
const JSClass kDirectoryClass = {"Directory", JSCLASS_HAS_RESERVED_SLOTS(2)};  // name, path
const char* const kMembers[] = {"getFilesAndDirectories", "getFiles", "get name", "get path"};
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  const std::string message = std::string("'") + kMembers[slot] +
      "' called on an object that does not implement interface Directory.";
  if (!WebKit::Check(cx, args.thisv(), &kDirectoryClass, &self, message.c_str())) {
    if (slot >= 2) return false;
    auto& state = *static_cast<State*>(WebKit::Owner(args));
    JS::RootedValue error(cx);
    if (!JS_GetPendingException(cx, &error)) return false;
    JS_ClearPendingException(cx);
    return state.kit.RejectWith(error, args.rval());
  }
  args.rval().set(JS::GetReservedSlot(self, uint32_t(slot - 2)));
  return true;
}
}  // namespace

bool InstallDirectory(State& state) {
  JS::RootedObject parent(state.cx, JS::GetRealmObjectPrototype(state.cx));
  WebFace face(state.cx);
  return state.kit.Define("Directory", &kDirectoryClass, parent, Illegal, &face) &&
         state.kit.Method(face, "Directory", "getFilesAndDirectories", Member, 0, 0) &&
         state.kit.Method(face, "Directory", "getFiles", Member, 0, 1) &&
         state.kit.Accessor(face, "Directory", "name", Member, nullptr, 2) &&
         state.kit.Accessor(face, "Directory", "path", Member, nullptr, 3);
}
}  // namespace ruyijs::web_platform
