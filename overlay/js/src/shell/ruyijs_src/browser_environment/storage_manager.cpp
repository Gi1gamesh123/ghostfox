// Firefox StorageManager surface behind navigator.storage. estimate() and
// persisted() answer the profile's validated "storage" group (baseline: the
// Firefox desktop quota, zero usage, not persisted). getDirectory() resolves
// the in-memory OPFS root owned by storage_opfs.cpp, matching Firefox's Worker
// access-handle surface.
#include "runtime/web_kit.h"
#include "browser_environment/storage_manager.h"
#include "browser_environment/state.h"
#include "runtime/runtime.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
#include "js/String.h"

namespace ruyijs::browser_environment {
namespace {
const JSClass kStorageManagerClass = {"StorageManager",
                                      JSCLASS_HAS_RESERVED_SLOTS(1)};
enum Member : int32_t { kEstimate = 0, kPersisted = 1, kPersist = 2, kDirectory = 3 };

bool IllegalConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgsFromVp(argc, vp);
  return Error(cx, "Illegal constructor.");
}

State* Owner(JSContext* cx, const JS::CallArgs& args) {
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  State* receiver = state ? ReceiverState(args, &kStorageManagerClass) : nullptr;
  if (!receiver) {
    WebKit::FailBrand(cx, args, "StorageManager");
    return nullptr;
  }
  return receiver;
}

bool Fulfill(JSContext* cx, JS::MutableHandleValue result,
             JS::HandleValue value) {
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !JS::ResolvePromise(cx, promise, value)) return false;
  result.setObject(*promise);
  return true;
}

// getDirectory() -> Promise<FileSystemDirectoryHandle>. One root per realm.
bool GetDirectory(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = Owner(cx, args);
  if (!state) return false;
  state->Observe("StorageManager", "getDirectory", int(ApiAccess::Call));
  if (!state->opfsStore) return Error(cx, "OPFS is not installed");
  JS::RootedObject root(cx, state->opfsStore->NewHandle(state->opfsStore->tree().root()));
  JS::RootedValue wrapped(cx, root ? JS::ObjectValue(*root) : JS::UndefinedHandleValue);
  if (!root || !Fulfill(cx, args.rval(), wrapped)) return false;
  return true;
}

bool Method(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  State* state = Owner(cx, args);
  if (!state) return false;
  const char* name = slot == kEstimate ? "estimate" :
                     slot == kPersisted ? "persisted" : "persist";
  state->Observe("StorageManager", name, int(ApiAccess::Call));
  // Estimate and persistence read the profile's "storage" group through the
  // environment model, the same owner that validated it (no second copy).
  JS::RootedValue quota(cx), usage(cx), persisted(cx);
  if (!state->Read(Kind::Storage, 0, ValueType::Number, &quota) ||
      !state->Read(Kind::Storage, 1, ValueType::Number, &usage) ||
      !state->Read(Kind::Storage, 2, ValueType::Boolean, &persisted)) return false;
  if (slot == kEstimate) {
    JS::RootedObject value(cx, JS_NewPlainObject(cx));
    if (!value) return false;
    // StorageEstimate is a WebIDL dictionary: a plain object whose members are
    // ordinary enumerable data properties, so JSON and key enumeration see them.
    // WebIDL member order is lexicographic: quota precedes usage.
    if (!JS_DefineProperty(cx, value, "quota", quota, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, value, "usage", usage, JSPROP_ENUMERATE)) return false;
    JS::RootedValue wrapped(cx, JS::ObjectValue(*value));
    return Fulfill(cx, args.rval(), wrapped);
  }
  return Fulfill(cx, args.rval(), persisted);
}

bool StorageGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  State* receiver = state ? ReceiverState(args, ClassFor(Kind::Navigator, state->worker)) : nullptr;
  if (!receiver) return Error(cx, "Navigator.storage called on incompatible receiver");
  state = receiver;
  state->Observe("Navigator", "storage", int(ApiAccess::Read));
  args.rval().setObject(*state->storageManager);
  return true;
}
}  // namespace

bool InstallStorageManager(State& state) {
  JSContext* cx = state.cx;
  JS::RootedObject prototype(cx, JS_NewPlainObject(cx));
  JS::RootedObject constructor(cx,
      state.Function(IllegalConstructor, "StorageManager", 0, 0,
                      JSFUN_CONSTRUCTOR));
  if (!prototype || !constructor) return false;
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, "StorageManager"));
  JS::RootedId tagId(cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  if (!tag || !JS_DefineProperty(cx, constructor, "prototype", prototype,
                                 JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(cx, prototype, "constructor", constructor, 0) ||
      !JS_DefinePropertyById(cx, prototype, tagId, tag, JSPROP_READONLY))
    return false;
  const char* names[] = {"estimate", "persisted", "persist"};
  // persist() is [Exposed=Window]; a Worker's StorageManager has the other two.
  for (int32_t slot = kEstimate; slot <= (state.worker ? kPersisted : kPersist); ++slot) {
    JS::RootedObject method(cx, state.Function(Method, names[slot], 0, slot));
    if (!method || !JS_DefineProperty(cx, prototype, names[slot], method, 0) ||
        !state.helpers->Member(method, "StorageManager", names[slot], "call"))
      return false;
  }
  JS::RootedObject dirMethod(cx, state.Function(GetDirectory, "getDirectory", 0, kDirectory));
  if (!dirMethod || !JS_DefineProperty(cx, prototype, "getDirectory", dirMethod, 0) ||
      !state.helpers->Member(dirMethod, "StorageManager", "getDirectory", "call"))
    return false;
  JS::RootedObject instance(cx, JS_NewObjectWithGivenProto(cx,
      &kStorageManagerClass, prototype));
  if (!instance) return false;
  JS::SetReservedSlot(instance, 0, JS::PrivateValue(&state));
  state.storageManager = instance;
  if (!JS_DefineProperty(cx, state.page, "StorageManager", constructor, 0) ||
      !state.helpers->Interface("StorageManager", prototype) ||
      !state.helpers->Object("StorageManager", prototype) ||
      !state.helpers->Constructor("StorageManager", constructor) ||
      !state.Accessor(state.navigatorPrototype, "storage", kDirectory,
                      JSPROP_ENUMERATE, StorageGetter) ||
      !state.helpers->Accessor("Navigator", state.navigatorPrototype,
                               "storage"))
    return false;
  return true;
}
}  // namespace ruyijs::browser_environment
