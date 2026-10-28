// FileReaderSync (docs/owners/WORKER.md, 2026-10-21): Firefox exposes it in
// dedicated and shared Workers only. Each method returns the value a FileReader
// read of the same Blob would deliver (ReadBlobPayload), synchronously; an
// unsupported readAsText label throws that EncodingError.
#include "web_platform/state.h"

#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::web_platform {
namespace {
const JSClass kReaderSyncClass = {"FileReaderSync", JSCLASS_HAS_RESERVED_SLOTS(1)};

bool Construct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  NativeConstructorTrace trace(cx, args);
  JS::RootedObject prototype(cx);
  if (!state.kit.ConstructProto(args, state.readerSync.prototype, &prototype)) return trace.Finish(false);
  JS::RootedObject object(cx, state.kit.New(&kReaderSyncClass, prototype));
  if (!object) return trace.Finish(false);
  args.rval().setObject(*object);
  return trace.Finish(true);
}

bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  const ReaderMethodKind kind = ReaderMethodKind(WebKit::Slot(args));
  JSObject* self = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (!self || JS::GetClass(self) != &kReaderSyncClass)
    return WebKit::Fail(cx, "FileReaderSync method called on an incompatible receiver");
  if (!argc)
    return WebKit::Fail(cx, "FileReaderSync read: At least 1 argument required, but only 0 passed");
  bool failed = false;
  if (!ReadBlobPayload(state, kind, args[0], args.get(1), args.rval(), &failed)) return false;
  if (failed) {
    JS_SetPendingException(cx, args.rval());
    return false;
  }
  return true;
}
}  // namespace

bool InstallFileReaderSync(State& state, PageHelpers& helpers) {
  JS::RootedObject objectPrototype(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!objectPrototype ||
      !state.kit.Define("FileReaderSync", &kReaderSyncClass, objectPrototype, Construct, &state.readerSync))
    return false;
  // Firefox 158 binding order.
  const struct { const char* name; unsigned length; ReaderMethodKind kind; } methods[] = {
      {"readAsArrayBuffer", 1, kReaderArrayBuffer}, {"readAsBinaryString", 1, kReaderBinaryString},
      {"readAsText", 1, kReaderText}, {"readAsDataURL", 1, kReaderDataURL}};
  for (const auto& method : methods)
    if (!state.kit.Method(state.readerSync, "FileReaderSync", method.name, Read, method.length, method.kind))
      return false;
  return helpers.Constructor("FileReaderSync", state.readerSync.constructor) &&
         helpers.Interface("FileReaderSync", state.readerSync.prototype);
}
}  // namespace ruyijs::web_platform
