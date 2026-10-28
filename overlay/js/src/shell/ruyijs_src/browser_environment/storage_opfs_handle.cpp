// FileSystemHandle (kind, name, isSameEntry, move) and FileSystemDirectoryHandle
// (getFileHandle, getDirectoryHandle, removeEntry, resolve, keys/values/entries and
// its async iterator) over the store's OpfsTree. Promise members reject, never throw.
#include "browser_environment/storage_opfs.h"
#include "browser_environment/state.h"
#include "runtime/runtime.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
#include <chrono>

namespace ruyijs::browser_environment {
namespace {
enum Member { kKind, kName, kSame, kMove, kGetFile, kGetDir, kRemove, kResolve, kKeys, kValues, kEntries, kNext };
State& Of(const JS::CallArgs& args) {
  return *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
}
double Now() {
  using namespace std::chrono;
  return double(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}
bool Text(JSContext* cx, JS::HandleValue value, std::string* out) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  return text && ToUTF8(cx, text, out);
}
bool Flag(JSContext* cx, JS::HandleValue options, const char* name, bool* out) {
  *out = false;
  if (!options.isObject()) return true;
  JS::RootedObject object(cx, &options.toObject());
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, object, name, &value)) return false;
  *out = JS::ToBoolean(value);
  return true;
}
bool StringValue(JSContext* cx, const std::string& text, JS::MutableHandleValue out) {
  JSString* s = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size()));
  if (!s) return false;
  out.setString(s);
  return true;
}
bool HandleMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Of(args);
  const int member = WebKit::Slot(args);
  const bool getter = member == kKind || member == kName;
  auto* node = OpfsNodeOf(args.thisv(), nullptr);
  if (!node) {
    WebKit::FailBrand(cx, args, "FileSystemHandle");
    return getter ? false : OpfsRejectPending(cx, args.rval());
  }
  s.Observe("FileSystemHandle", member == kKind ? "kind" : member == kName ? "name" :
            member == kSame ? "isSameEntry" : "move", 0);
  if (member == kKind) return StringValue(cx, (*node)->directory ? "directory" : "file", args.rval());
  if (member == kName) return StringValue(cx, (*node)->name, args.rval());
  if (member == kSame) {
    auto* other = OpfsNodeOf(args.get(0), nullptr);
    if (!other) {
      Error(cx, "FileSystemHandle.isSameEntry: Argument 1 does not implement interface FileSystemHandle.");
      return OpfsRejectPending(cx, args.rval());
    }
    JS::RootedValue same(cx, JS::BooleanValue(other->get() == node->get()));
    return OpfsResolve(cx, same, args.rval());
  }
  std::string name;
  if (!Text(cx, args.get(0), &name)) return OpfsRejectPending(cx, args.rval());
  return OpfsRejectStatus(s, s.opfsStore->tree().Rename(*node, name), args.rval());
}
bool DirectoryMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Of(args);
  const int member = WebKit::Slot(args);
  auto* node = OpfsNodeOf(args.thisv(), &kOpfsDirectoryClass);
  if (!node) {
    WebKit::FailBrand(cx, args, "FileSystemDirectoryHandle");
    return member >= kKeys ? false : OpfsRejectPending(cx, args.rval());
  }
  OpfsTree& tree = s.opfsStore->tree();
  if (member == kKeys || member == kValues || member == kEntries) {
    JS::RootedObject names(cx, JS::NewArrayObject(cx, 0));
    if (!names) return false;
    uint32_t index = 0;
    JS::RootedValue name(cx);
    for (const auto& child : (*node)->children)
      if (!StringValue(cx, child.first, &name) || !JS_SetElement(cx, names, index++, name)) return false;
    JS::RootedObject iterator(cx, OpfsNewObject(s, &kOpfsIteratorClass, kOpfsIterator, *node));
    if (!iterator) return false;
    JS::SetReservedSlot(iterator, 1, JS::ObjectValue(*names));
    JS::SetReservedSlot(iterator, 2, JS::Int32Value(0));
    JS::SetReservedSlot(iterator, 3, JS::Int32Value(member));
    args.rval().setObject(*iterator);
    return true;
  }
  std::string name;
  if (member == kResolve) {
    auto* target = OpfsNodeOf(args.get(0), nullptr);
    if (!target) {
      Error(cx, "FileSystemDirectoryHandle.resolve: Argument 1 does not implement interface FileSystemHandle.");
      return OpfsRejectPending(cx, args.rval());
    }
    std::vector<std::string> path;
    JS::RootedValue result(cx, JS::NullValue());
    if (OpfsTree::Resolve(**node, **target, &path)) {
      JS::RootedObject list(cx, JS::NewArrayObject(cx, path.size()));
      JS::RootedValue part(cx);
      if (!list) return false;
      for (size_t i = 0; i < path.size(); ++i)
        if (!StringValue(cx, path[i], &part) || !JS_SetElement(cx, list, uint32_t(i), part)) return false;
      result.setObject(*list);
    }
    return OpfsResolve(cx, result, args.rval());
  }
  bool flag = false;
  if (!Text(cx, args.get(0), &name) ||
      !Flag(cx, args.get(1), member == kRemove ? "recursive" : "create", &flag))
    return OpfsRejectPending(cx, args.rval());
  if (member == kRemove) return OpfsRejectStatus(s, tree.Remove(*node, name, flag), args.rval());
  std::shared_ptr<OpfsNode> child;
  const OpfsStatus status = tree.Child(*node, name, member == kGetDir, flag, Now(), &child);
  if (status != OpfsStatus::Ok) return OpfsRejectStatus(s, status, args.rval());
  JS::RootedObject handle(cx, s.opfsStore->NewHandle(child));
  if (!handle) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*handle));
  return OpfsResolve(cx, value, args.rval());
}
// next() of the directory iterator: the names snapshot taken at keys()/values()/
// entries(), skipping names removed since; resolves {value, done}.
bool IteratorNext(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Of(args);
  JSObject* raw = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (!raw || JS::GetClass(raw) != &kOpfsIteratorClass) {
    Error(cx, "next called on an object that does not implement interface FileSystemDirectoryHandle AsyncIterator.");
    return OpfsRejectPending(cx, args.rval());
  }
  JS::RootedObject self(cx, raw), names(cx, &JS::GetReservedSlot(raw, 1).toObject());
  auto& node = *static_cast<std::shared_ptr<OpfsNode>*>(JS::GetReservedSlot(raw, 0).toPrivate());
  const int kind = JS::GetReservedSlot(raw, 3).toInt32();
  uint32_t length = 0, index = uint32_t(JS::GetReservedSlot(raw, 2).toInt32());
  if (!JS::GetArrayLength(cx, names, &length)) return false;
  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  JS::RootedValue name(cx), value(cx), done(cx, JS::TrueValue());
  if (!result) return false;
  for (; index < length; ++index) {
    std::string key;
    if (!JS_GetElement(cx, names, index, &name) || !Text(cx, name, &key)) return false;
    auto found = node->children.find(key);
    if (found == node->children.end()) continue;
    JS::RootedObject handle(cx, s.opfsStore->NewHandle(found->second));
    if (!handle) return false;
    if (kind == kKeys) value.set(name);
    else if (kind == kValues) value.setObject(*handle);
    else {
      JS::RootedValueArray<2> pair(cx);
      pair[0].set(name);
      pair[1].setObject(*handle);
      JS::RootedObject entry(cx, JS::NewArrayObject(cx, pair));
      if (!entry) return false;
      value.setObject(*entry);
    }
    done.setBoolean(false);
    ++index;
    break;
  }
  JS::SetReservedSlot(self, 2, JS::Int32Value(int32_t(index)));
  if (!JS_DefineProperty(cx, result, "value", value, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, result, "done", done, JSPROP_ENUMERATE)) return false;
  JS::RootedValue resolved(cx, JS::ObjectValue(*result));
  return OpfsResolve(cx, resolved, args.rval());
}
bool Method(State& s, JS::HandleObject target, JSNative native, const char* name, unsigned length, int slot,
            JS::MutableHandleObject made) {
  made.set(s.Function(native, name, length, slot));
  return made && JS_DefineProperty(s.cx, target, name, made, JSPROP_ENUMERATE);
}
}  // namespace

bool InstallOpfsHandleMembers(State& s, JS::HandleObject handle, JS::HandleObject file,
                              JS::HandleObject directory) {
  JSContext* cx = s.cx;
  JS::RootedObject fn(cx), entries(cx);
  if (!Method(s, handle, HandleMember, "move", 1, kMove, &fn) ||
      !Method(s, handle, HandleMember, "isSameEntry", 1, kSame, &fn) ||
      !s.Accessor(handle, "kind", kKind, JSPROP_ENUMERATE, HandleMember) ||
      !s.Accessor(handle, "name", kName, JSPROP_ENUMERATE, HandleMember) ||
      !Method(s, file, OpfsGetFile, "getFile", 0, 0, &fn) ||
      !Method(s, file, OpfsCreateWritable, "createWritable", 0, 0, &fn) ||
      (s.worker && s.dedicatedWorker &&
       !Method(s, file, OpfsCreateSyncAccessHandle, "createSyncAccessHandle", 0, 0, &fn)) ||
      !Method(s, directory, DirectoryMember, "getFileHandle", 1, kGetFile, &fn) ||
      !Method(s, directory, DirectoryMember, "getDirectoryHandle", 1, kGetDir, &fn) ||
      !Method(s, directory, DirectoryMember, "removeEntry", 1, kRemove, &fn) ||
      !Method(s, directory, DirectoryMember, "resolve", 1, kResolve, &fn) ||
      !Method(s, directory, DirectoryMember, "values", 0, kValues, &fn) ||
      !Method(s, directory, DirectoryMember, "entries", 0, kEntries, &entries) ||
      !Method(s, directory, DirectoryMember, "keys", 0, kKeys, &fn)) return false;
  // WebIDL async iterable: @@asyncIterator is the entries function itself.
  JS::RootedId asyncId(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::asyncIterator)));
  JS::RootedValue entriesValue(cx, JS::ObjectValue(*entries));
  if (!JS_DefinePropertyById(cx, directory, asyncId, entriesValue, 0)) return false;
  // The iterator prototype: next + toStringTag, inheriting %AsyncIteratorPrototype%.
  JS::RootedObject asyncProto(cx, JS::GetRealmAsyncIteratorPrototype(cx));
  JS::RootedObject iterator(cx, asyncProto ? JS_NewObjectWithGivenProto(cx, nullptr, asyncProto) : nullptr);
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, "FileSystemDirectoryHandle AsyncIterator"));
  JS::RootedId tagId(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  if (!iterator || !tag || !Method(s, iterator, IteratorNext, "next", 0, kNext, &fn) ||
      !JS_DefinePropertyById(cx, iterator, tagId, tag, JSPROP_READONLY)) return false;
  s.opfsStore->SetPrototype(kOpfsIterator, iterator);
  return true;
}
}  // namespace ruyijs::browser_environment
