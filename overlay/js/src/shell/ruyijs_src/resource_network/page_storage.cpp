#include "runtime/web_kit.h"
#include "resource_network/state.h"
#include "resource_network/body_acquire.h"
#include "resource_network/xhr.h"
#include "resource_network/abort.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"
#include "js/Conversions.h"
#include "js/CharacterEncoding.h"
#include "js/String.h"
#include <string>

namespace ruyijs::resource_network {
// Instance identity for localStorage and sessionStorage. Slot 0 keeps the one
// StorageArea this page owns, slot 1 selects which area; both are set by the
// unique installer and never by script.
static const JSClass kStorageClassDefinition = {"Storage", JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kStorageClass = kStorageClassDefinition;

State::~State() { DetachBodyOperations(*this); DetachXhrs(*this); DetachAbortSignals(*this); Release(arena); if (owns_store) DeleteStorage(store); }

enum class StorageOp { GetItem, SetItem, RemoveItem, Clear, Key, Length };

// The native function holds the owning State in reserved slot 0, so a method
// reports through that realm's port and reads the one store it owns.
static State* Owner(const JS::CallArgs& args) {
  return static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
}
static StorageArea* AreaOf(const JS::CallArgs& args) { return Owner(args)->store; }
// The installed member name of each Storage operation; static names only.
static const char* StorageName(StorageOp op) {
  switch (op) {
    case StorageOp::GetItem: return "getItem";
    case StorageOp::SetItem: return "setItem";
    case StorageOp::RemoveItem: return "removeItem";
    case StorageOp::Clear: return "clear";
    case StorageOp::Key: return "key";
    case StorageOp::Length: return "length";
  }
  return "";
}
static StorageOp OpOf(const JS::CallArgs& args) {
  return StorageOp(js::GetFunctionNativeReserved(&args.callee(), 1).toInt32());
}
static JSObject* Native(State& s, JSNative callback, const char* name,
                        unsigned length, StorageOp op, unsigned flags = 0) {
  JS::RootedFunction fn(s.cx, js::NewFunctionWithReserved(s.cx, callback, length, flags, name));
  if (!fn) return nullptr;
  JS::RootedObject object(s.cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(&s));
  js::SetFunctionNativeReserved(object, 1, JS::Int32Value(int32_t(op)));
  return object;
}

// A prototype method requires a real Storage receiver, so calling it on a plain
// object throws instead of reading another page's state.
static bool Which(JSContext* cx, const JS::CallArgs& args, bool* session) {
  if (args.thisv().isObject()) {
    JSObject* receiver = &args.thisv().toObject();
    if (JS::GetClass(receiver) == &kStorageClass) {
      *session = JS::GetReservedSlot(receiver, 1).toInt32() != 0;
      return true;
    }
  }
  WebKit::FailBrand(cx, args, "Storage");
  return false;
}

// Storage keys and values are DOMStrings (Firefox), held as UTF-8 by the store,
// the same encoding profile/host imports use. A lone surrogate becomes U+FFFD.
static bool Text(JSContext* cx, JS::HandleValue input, std::string* result) {
  JS::RootedString text(cx, JS::ToString(cx, input));
  if (!text) return false;
  JSLinearString* linear = JS_EnsureLinearString(cx, text);
  if (!linear) return false;
  // Sized, not NUL-terminated: a value may contain U+0000.
  result->resize(JS::GetDeflatedUTF8StringLength(linear));
  JS::DeflateStringToUTF8Buffer(linear, mozilla::Span<char>(result->data(), result->size()));
  return true;
}
static bool Text(JSContext* cx, const std::string& source, JS::MutableHandleValue result) {
  JSString* text = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(source.data(), source.size()));
  if (!text) return false;
  result.setString(text);
  return true;
}

static bool Access(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  bool session = false;
  if (!Which(cx, args, &session)) return false;
  // Reported after receiver validation, so an invalid receiver still throws
  // from the same place and records nothing.
  const StorageOp operation = OpOf(args);
  Owner(args)->Observe("Storage", StorageName(operation),
                       operation == StorageOp::Length ? int(ApiAccess::Read)
                                                      : int(ApiAccess::Call));
  StorageArea* area = AreaOf(args);
  std::string key, value;
  bool found = false;
  switch (operation) {
    case StorageOp::GetItem:
      if (!argc || !Text(cx, args[0], &key)) return Error(cx, "Storage.getItem requires a key");
      if (!Report(cx, StorageGet(area, session, key, &value, &found))) return false;
      if (!found) { args.rval().setNull(); return true; }
      return Text(cx, value, args.rval());
    case StorageOp::SetItem:
      if (argc < 2 || !Text(cx, args[0], &key) || !Text(cx, args[1], &value))
        return Error(cx, "Storage.setItem requires a key and value");
      if (!Report(cx, StorageSet(area, session, key, value))) return false;
      args.rval().setUndefined();
      return true;
    case StorageOp::RemoveItem:
      if (!argc || !Text(cx, args[0], &key)) return Error(cx, "Storage.removeItem requires a key");
      if (!Report(cx, StorageRemove(area, session, key))) return false;
      args.rval().setUndefined();
      return true;
    case StorageOp::Clear:
      if (!Report(cx, StorageClear(area, session))) return false;
      args.rval().setUndefined();
      return true;
    case StorageOp::Key: {
      double index = 0;
      if (!argc || !JS::ToNumber(cx, args[0], &index))
        return Error(cx, "Storage.key requires an index");
      // A negative or non-finite index is out of range, never a wrapped offset.
      if (!(index >= 0) || index != index) { args.rval().setNull(); return true; }
      if (!Report(cx, StorageKeyAt(area, session, size_t(index), &key, &found))) return false;
      if (!found) { args.rval().setNull(); return true; }
      return Text(cx, key, args.rval());
    }
    case StorageOp::Length: {
      size_t length = 0;
      if (!Report(cx, StorageLength(area, session, &length))) return false;
      args.rval().setInt32(int32_t(length));
      return true;
    }
  }
  return Error(cx, "Unknown Storage operation");
}

static bool StorageConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return Error(cx, "Illegal constructor.");
}

// One instance per area, created by this installer from the realm's own
// prototype; script cannot construct another.
static bool Instance(State& s, JS::HandleObject prototype, const char* name, bool session) {
  JSContext* cx = s.cx;
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kStorageClass, prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, 0, JS::PrivateValue(s.store));
  JS::SetReservedSlot(object, 1, JS::Int32Value(session ? 1 : 0));
  return JS_DefineProperty(cx, s.page, name, object, JSPROP_ENUMERATE);
}

static bool InstallStorageInterface(State& s, PageHelpers& helpers) {
  JSContext* cx = s.cx;
  JS::RootedObject prototype(cx, JS_NewPlainObject(cx));
  JS::RootedObject constructor(cx, Native(s, StorageConstructor, "Storage", 0,
                                          StorageOp::Length, JSFUN_CONSTRUCTOR));
  if (!prototype || !constructor ||
      !JS_DefineProperty(cx, constructor, "prototype", prototype,
                         JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(cx, prototype, "constructor", constructor, 0) ||
      !Tag(cx, prototype, "Storage")) return false;
  struct Definition { const char* name; unsigned length; StorageOp op; };
  static const Definition definitions[] = {
    {"getItem", 1, StorageOp::GetItem}, {"setItem", 2, StorageOp::SetItem},
    {"removeItem", 1, StorageOp::RemoveItem}, {"clear", 0, StorageOp::Clear},
    {"key", 1, StorageOp::Key}
  };
  for (const auto& definition : definitions) {
    JS::RootedObject method(cx, Native(s, Access, definition.name, definition.length, definition.op));
    if (!method || !JS_DefineProperty(cx, prototype, definition.name, method, JSPROP_ENUMERATE) ||
        !helpers.Member(method, "Storage", definition.name, "call")) return false;
  }
  JS::RootedObject length(cx, Native(s, Access, "get length", 0, StorageOp::Length));
  if (!length || !JS_DefineProperty(cx, prototype, "length", length, nullptr, JSPROP_ENUMERATE) ||
      !helpers.Member(length, "Storage", "get length", "get") ||
      !helpers.Constructor("Storage", constructor) ||
      !JS_DefineProperty(cx, s.page, "Storage", constructor, 0)) return false;
  return Instance(s, prototype, "localStorage", false) &&
         Instance(s, prototype, "sessionStorage", true);
}

bool InstallStorage(State& s, PageHelpers& helpers) {
  // A child realm's view already points at the owning realm's store and must
  // keep sharing it; only the realm that owns the store creates it.
  if (!s.store && !Report(s.cx, NewStorage(&s.store))) return false;
  JSContext* cx = s.cx;
  JS::RootedValue document(cx), prototype(cx);
  JS::RootedObject constructor(cx), proto(cx);
  if (!JS_GetProperty(cx, s.page, "Document", &document) ||
      !document.isObject()) return false;
  constructor = &document.toObject();
  if (!JS_GetProperty(cx, constructor, "prototype", &prototype) || !prototype.isObject())
    return false;
  proto = &prototype.toObject();
  return InstallCookie(s,helpers,proto) &&
         InstallStorageInterface(s, helpers) && InstallCookieStore(s,helpers);
}
}
