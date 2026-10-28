// Origin private file system: the store, the handle classes and the one installer
// of the File System interfaces (Firefox 158: FileSystemHandle, -FileHandle,
// -DirectoryHandle, -WritableFileStream on Window and every Worker kind;
// FileSystemSyncAccessHandle in dedicated Workers only).
#include "browser_environment/storage_opfs.h"
#include "browser_environment/state.h"
#include "browser_environment/window_members.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
#include "js/WeakMap.h"
#include "js/Wrapper.h"
#include <chrono>
#include <new>

namespace ruyijs::browser_environment {
namespace {
void FinalizeNode(JS::GCContext*, JSObject* object) {
  const JS::Value slot = JS::GetReservedSlot(object, 0);
  if (!slot.isUndefined()) delete static_cast<std::shared_ptr<OpfsNode>*>(slot.toPrivate());
}
const JSClassOps kNodeOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, FinalizeNode};
bool Illegal(JSContext* cx, unsigned, JS::Value*) { return Error(cx, "Illegal constructor."); }
}  // namespace
// Slot 0: the node holder (heap shared_ptr, freed by the finalizer); later slots per class.
const JSClass kOpfsFileClass = {"FileSystemFileHandle",
    JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_FOREGROUND_FINALIZE, &kNodeOps};
const JSClass kOpfsDirectoryClass = {"FileSystemDirectoryHandle",
    JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_FOREGROUND_FINALIZE, &kNodeOps};
const JSClass kOpfsIteratorClass = {"FileSystemDirectoryHandle AsyncIterator",
    JSCLASS_HAS_RESERVED_SLOTS(4) | JSCLASS_FOREGROUND_FINALIZE, &kNodeOps};

OpfsStore::OpfsStore(JSContext* cx, State& state)
    : cx_(cx), state_(state) {
  for (auto& prototype : prototypes_) prototype.init(cx);
  writables_.init(cx);
}
bool OpfsStore::MarkWritable(JS::HandleObject stream) {
  if (!writables_) writables_ = JS::NewWeakMapObject(cx_);
  JS::RootedObject map(cx_, writables_);
  JS::RootedValue key(cx_, JS::ObjectValue(*stream));
  return map && JS::SetWeakMapEntry(cx_, map, key, JS::TrueHandleValue);
}
bool OpfsStore::IsWritable(JS::HandleValue value) {
  if (!value.isObject() || !writables_) return false;
  JS::RootedObject map(cx_, writables_);
  JS::RootedValue key(cx_, value), found(cx_);
  return JS::GetWeakMapEntry(cx_, map, key, &found) && found.isTrue();
}
void OpfsStore::Observe(const char* iface, const char* member, int type) const {
  state_.Observe(iface, member, type);
}
JSObject* OpfsNewObject(State& state, const JSClass* clasp, OpfsFace face,
                        const std::shared_ptr<OpfsNode>& node) {
  JSContext* cx = state.cx;
  JS::RootedObject prototype(cx, state.opfsStore->prototype(face));
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, clasp, prototype));
  if (!object) return nullptr;
  auto* holder = new (std::nothrow) std::shared_ptr<OpfsNode>(node);
  if (!holder) { JS_ReportOutOfMemory(cx); return nullptr; }
  JS::SetReservedSlot(object, 0, JS::PrivateValue(holder));
  return object;
}
JSObject* OpfsStore::NewHandle(const std::shared_ptr<OpfsNode>& node) {
  return node->directory ? OpfsNewObject(state_, &kOpfsDirectoryClass, kOpfsDirectory, node)
                         : OpfsNewObject(state_, &kOpfsFileClass, kOpfsFile, node);
}
std::shared_ptr<OpfsNode>* OpfsNodeOf(JS::HandleValue value, const JSClass* clasp) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  if (!object) return nullptr;
  const JSClass* actual = JS::GetClass(object);
  const bool match = clasp ? actual == clasp : (actual == &kOpfsFileClass || actual == &kOpfsDirectoryClass);
  if (!match) return nullptr;
  const JS::Value slot = JS::GetReservedSlot(object, 0);
  return slot.isUndefined() ? nullptr : static_cast<std::shared_ptr<OpfsNode>*>(slot.toPrivate());
}
OpfsStore* OpfsStoreOf(const JS::CallArgs& args) {
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  return state ? state->opfsStore : nullptr;
}
bool OpfsResolve(JSContext* cx, JS::HandleValue value, JS::MutableHandleValue rval) {
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !JS::ResolvePromise(cx, promise, value)) return false;
  rval.setObject(*promise);
  return true;
}
bool OpfsRejectPending(JSContext* cx, JS::MutableHandleValue rval) {
  JS::RootedValue reason(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx, &reason)) return false;
  JS_ClearPendingException(cx);
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !JS::RejectPromise(cx, promise, reason)) return false;
  rval.setObject(*promise);
  return true;
}
bool OpfsReject(State& state, const char* name, const char* message, JS::MutableHandleValue rval) {
  if (!std::string(name).compare("TypeError")) Error(state.cx, message);
  else ThrowDOM(state, name, message);
  return OpfsRejectPending(state.cx, rval);
}
bool OpfsRejectStatus(State& state, OpfsStatus status, JS::MutableHandleValue rval) {
  switch (status) {
    case OpfsStatus::NotFound: return OpfsReject(state, "NotFoundError", "Entry not found", rval);
    case OpfsStatus::TypeMismatch: return OpfsReject(state, "TypeMismatchError", "Wrong type", rval);
    case OpfsStatus::InvalidName: return OpfsReject(state, "TypeError", "Invalid filename", rval);
    case OpfsStatus::NotEmpty:
      return OpfsReject(state, "InvalidModificationError", "Invalid modification", rval);
    case OpfsStatus::Ok: break;
  }
  return OpfsResolve(state.cx, JS::UndefinedHandleValue, rval);
}

bool OpfsInterface(State& s, const char* name, JS::HandleObject parentProto, JS::HandleObject parentCtor,
               JS::MutableHandleObject prototype) {
  JSContext* cx = s.cx;
  prototype.set(JS_NewObjectWithGivenProto(cx, nullptr, parentProto));
  JS::RootedObject ctor(cx, s.Function(Illegal, name, 0, 0, JSFUN_CONSTRUCTOR));
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, name));
  JS::RootedId tagId(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  return prototype && ctor && tag && (!parentCtor || JS_SetPrototype(cx, ctor, parentCtor)) &&
         JS_DefineProperty(cx, ctor, "prototype", prototype, JSPROP_READONLY | JSPROP_PERMANENT) &&
         JS_DefineProperty(cx, prototype, "constructor", ctor, 0) &&
         JS_DefinePropertyById(cx, prototype, tagId, tag, JSPROP_READONLY) &&
         JS_DefineProperty(cx, s.page, name, ctor, 0) &&
         s.helpers->Interface(name, prototype) && s.helpers->Constructor(name, ctor);
}
namespace {
bool GlobalPrototype(State& s, const char* name, JS::MutableHandleObject out) {
  JS::RootedValue ctor(s.cx), proto(s.cx);
  if (!JS_GetProperty(s.cx, s.page, name, &ctor) || !ctor.isObject()) return Error(s.cx, "OPFS parent missing");
  JS::RootedObject object(s.cx, &ctor.toObject());
  if (!JS_GetProperty(s.cx, object, "prototype", &proto) || !proto.isObject()) return false;
  out.set(&proto.toObject());
  return true;
}
}  // namespace

bool InstallOpfs(State& s) {
  JSContext* cx = s.cx;
  if (!s.opfsStore) {
    s.opfsStore = new (std::nothrow) OpfsStore(cx, s);
    if (!s.opfsStore) { JS_ReportOutOfMemory(cx); return false; }
  }
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx)), none(cx);
  JS::RootedObject handle(cx), file(cx), directory(cx), writable(cx), sync(cx), streamProto(cx);
  JS::RootedValue handleCtor(cx), streamCtor(cx);
  if (!OpfsInterface(s, "FileSystemHandle", objectProto, none, &handle) ||
      !JS_GetProperty(cx, s.page, "FileSystemHandle", &handleCtor)) return false;
  JS::RootedObject handleCtorObject(cx, &handleCtor.toObject());
  if (!OpfsInterface(s, "FileSystemFileHandle", handle, handleCtorObject, &file) ||
      !OpfsInterface(s, "FileSystemDirectoryHandle", handle, handleCtorObject, &directory) ||
      !GlobalPrototype(s, "WritableStream", &streamProto) ||
      !JS_GetProperty(cx, s.page, "WritableStream", &streamCtor)) return false;
  JS::RootedObject streamCtorObject(cx, &streamCtor.toObject());
  if (!OpfsInterface(s, "FileSystemWritableFileStream", streamProto, streamCtorObject, &writable)) return false;
  OpfsStore& store = *s.opfsStore;
  store.SetPrototype(kOpfsHandle, handle);
  store.SetPrototype(kOpfsFile, file);
  store.SetPrototype(kOpfsDirectory, directory);
  store.SetPrototype(kOpfsWritable, writable);
  if (!InstallOpfsHandleMembers(s, handle, file, directory) || !InstallOpfsWritable(s, writable)) return false;
  if (s.worker && s.dedicatedWorker) {
    if (!OpfsInterface(s, "FileSystemSyncAccessHandle", objectProto, none, &sync)) return false;
    store.SetPrototype(kOpfsSync, sync);
    if (!InstallOpfsSync(s, sync)) return false;
  }
  return s.worker || InstallOpfsEntries(s);
}
}  // namespace ruyijs::browser_environment
