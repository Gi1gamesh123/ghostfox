// FileList (dom/webidl/FileList.webidl, Exposed=(Window,Worker)): a read-only list of
// File objects with item(), length, indexed properties and @@iterator. Not
// constructible; instances come from this owner's factory (Realm::NewFileList), used by
// HTMLInputElement.files and DataTransfer.files through their ports.
#include "web_platform/state.h"
#include "web_platform/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Symbol.h"

namespace ruyijs::web_platform {
namespace {
// Slot 0: the frozen array of files the list was made from.
const JSClass kFileListClass = {"FileList", JSCLASS_HAS_RESERVED_SLOTS(1)};
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
bool Files(JSContext* cx, const JS::CallArgs& args, const char* member, JS::MutableHandleObject files) {
  JS::RootedObject self(cx);
  const std::string message = std::string("'") + member + "' called on an object that does not implement interface FileList.";
  if (!WebKit::Check(cx, args.thisv(), &kFileListClass, &self, message.c_str())) return false;
  files.set(&JS::GetReservedSlot(self, 0).toObject());
  return true;
}
bool Item(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject files(cx);
  if (!Files(cx, args, "item", &files)) return false;
  if (!argc) return WebKit::Fail(cx, "FileList.item: At least 1 argument required, but only 0 passed");
  uint32_t index = 0, length = 0;
  if (!JS::ToUint32(cx, args[0], &index) || !JS::GetArrayLength(cx, files, &length)) return false;
  if (index >= length) { args.rval().setNull(); return true; }
  return JS_GetElement(cx, files, index, args.rval()) && JS_WrapValue(cx, args.rval());
}
bool Length(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject files(cx);
  uint32_t length = 0;
  if (!Files(cx, args, "get length", &files) || !JS::GetArrayLength(cx, files, &length)) return false;
  args.rval().setNumber(length);
  return true;
}
}  // namespace

bool InstallFileList(State& state) {
  JSContext* cx = state.cx;
  JS::RootedObject parent(cx, JS::GetRealmObjectPrototype(cx));
  if (!state.kit.Define("FileList", &kFileListClass, parent, Illegal, &state.fileList) ||
      !state.kit.Method(state.fileList, "FileList", "item", Item, 1, 0) ||
      !state.kit.Accessor(state.fileList, "FileList", "length", Length, nullptr, 0)) return false;
  // WebIDL indexed getter + integer length: @@iterator is %Array.prototype.values%.
  JS::RootedObject arrayPrototype(cx, JS::GetRealmArrayPrototype(cx));
  JS::RootedValue values(cx);
  JS::RootedId iterator(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
  JS::RootedObject prototype(cx, state.fileList.prototype);
  return arrayPrototype && JS_GetProperty(cx, arrayPrototype, "values", &values) &&
         JS_DefinePropertyById(cx, prototype, iterator, values, 0);
}

bool Realm::NewFileList(JS::HandleObject files, JS::MutableHandleObject result) {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedObject source(cx, files);
  if (source && !JS_WrapObject(cx, &source)) return false;
  uint32_t length = 0;
  if (source && !JS::GetArrayLength(cx, source, &length)) return false;
  JS::RootedObject copy(cx, JS::NewArrayObject(cx, length));
  JS::RootedObject prototype(cx, s.fileList.prototype);
  JS::RootedObject list(cx, s.kit.New(&kFileListClass, prototype));
  if (!copy || !list) return false;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue file(cx);
    if (!JS_GetElement(cx, source, i, &file) || !JS_SetElement(cx, copy, i, file) ||
        !JS_DefineElement(cx, list, i, file, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) return false;
  }
  if (!JS_FreezeObject(cx, copy)) return false;
  JS::SetReservedSlot(list, 0, JS::ObjectValue(*copy));
  result.set(list);
  return true;
}
}  // namespace ruyijs::web_platform
