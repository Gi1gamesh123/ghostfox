#include "resource_network/state.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
#include <memory>
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Symbol.h"

namespace ruyijs::resource_network {
static void FinalizeHeaders(JS::GCContext*, JSObject* object) {
  auto slot = JS::GetReservedSlot(object, 0);
  if (!slot.isUndefined()) DeleteHeaders(static_cast<HeaderRecord*>(slot.toPrivate()));
}
static void FinalizeIterator(JS::GCContext*, JSObject* object) {
  auto slot = JS::GetReservedSlot(object, 3);
  if (!slot.isUndefined()) DeleteTicket(static_cast<IteratorTicket*>(slot.toPrivate()));
}
static void FinalizeFactory(JS::GCContext*, JSObject* object) {
  auto slot = JS::GetReservedSlot(object, 0);
  if (!slot.isUndefined()) Release(static_cast<HeaderArena*>(slot.toPrivate()));
}
static const JSClassOps kHeadersOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, FinalizeHeaders};
static const JSClassOps kIteratorOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, FinalizeIterator};
static const JSClassOps kFactoryOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, FinalizeFactory};
const JSClass kHeadersClass = {"Headers", JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_FOREGROUND_FINALIZE, &kHeadersOps};
const JSClass kIteratorClass = {"Headers Iterator", JSCLASS_HAS_RESERVED_SLOTS(4) | JSCLASS_FOREGROUND_FINALIZE, &kIteratorOps};
const JSClass kFactoryClass = {"Headers Factory", JSCLASS_HAS_RESERVED_SLOTS(4) | JSCLASS_FOREGROUND_FINALIZE, &kFactoryOps};

static JSObject* Function(State& s, JSNative callback, const char* name, unsigned length,
                           Operation op, unsigned flags = 0) {
  JS::RootedFunction fn(s.cx, js::NewFunctionWithReserved(s.cx, callback, length, flags, name));
  if (!fn) return nullptr;
  JS::RootedObject object(s.cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(object, 0, JS::ObjectValue(*s.holder));
  js::SetFunctionNativeReserved(object, 1, JS::Int32Value(int32_t(op)));
  return object;
}
// The factory object holds the owning State in reserved slot 3, so the
// Headers methods and iterator report through the same owner port without a
// second pointer in each native function.
State* FactoryOwner(const JS::CallArgs& args) {
  auto holder = js::GetFunctionNativeReserved(&args.callee(), 0);
  if (!holder.isObject()) return nullptr;
  auto slot = JS::GetReservedSlot(&holder.toObject(), 3);
  return slot.isUndefined() ? nullptr : static_cast<State*>(slot.toPrivate());
}
// The installed member name of each Headers operation. Static names only, so a
// record never carries script-supplied text.
const char* OperationName(Operation operation) {
  switch (operation) {
    case Operation::Append: return "append";
    case Operation::Delete: return "delete";
    case Operation::Get: return "get";
    case Operation::Cookies: return "getSetCookie";
    case Operation::Has: return "has";
    case Operation::Set: return "set";
    case Operation::Entries: return "entries";
    case Operation::Keys: return "keys";
    case Operation::Values: return "values";
    case Operation::ForEach: return "forEach";
  }
  return "";
}
bool Tag(JSContext* cx, JS::HandleObject object, const char* text) {
  JS::RootedId id(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  JS::RootedString value(cx, JS_NewStringCopyZ(cx, text));
  return value && JS_DefinePropertyById(cx, object, id, value, JSPROP_READONLY);
}
// The Headers owner builds an object of its own class from already-converted
// pairs: Request and Response reuse it instead of holding a header container.
bool HeaderObject(State& s, const HeaderPairs& pairs, JS::MutableHandleObject result, bool immutable) {
  JSContext* cx = s.cx;
  HeaderDraft* raw = nullptr;
  if (!Report(cx, NewDraft(&raw))) return false;
  std::unique_ptr<HeaderDraft, decltype(&DeleteDraft)> draft(raw, DeleteDraft);
  JS::RootedObject prototype(cx, &JS::GetReservedSlot(s.holder, 1).toObject());
  if (!prototype) return false;
  for (const auto& pair : pairs)
    if (!Report(cx, DraftTuple(raw)) || !Report(cx, DraftValue(raw, pair.first)) ||
        !Report(cx, DraftValue(raw, pair.second))) return false;
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kHeadersClass, prototype));
  if (!object) return false;
  HeaderRecord* record = nullptr;
  if (!Report(cx, CreateHeaders(s.arena, raw, &record))) return false;
  if (immutable) MakeHeadersImmutable(record);
  JS::SetReservedSlot(object, 0, JS::PrivateValue(record));
  result.set(object);
  return true;
}

bool InstallHeaders(State& s, PageHelpers& helpers) {
  JSContext* cx = s.cx;
  s.holder = JS_NewObjectWithGivenProto(cx, &kFactoryClass, nullptr);
  if (!s.holder) return false;
  Retain(s.arena); JS::SetReservedSlot(s.holder, 0, JS::PrivateValue(s.arena));
  JS::SetReservedSlot(s.holder, 3, JS::PrivateValue(&s));
  JS::RootedObject prototype(cx, JS_NewPlainObject(cx));
  JS::RootedObject parent(cx, JS::GetRealmIteratorPrototype(cx));
  JS::RootedObject iterator(cx, JS_NewPlainObject(cx));
  if (!prototype || !parent || !iterator || !JS_SetPrototype(cx, iterator, parent)) return false;
  JS::SetReservedSlot(s.holder, 1, JS::ObjectValue(*prototype));
  JS::SetReservedSlot(s.holder, 2, JS::ObjectValue(*iterator));
  JS::RootedObject constructor(cx, Function(s, Constructor, "Headers", 0, Operation::Append, JSFUN_CONSTRUCTOR));
  if (!constructor || !JS_DefineProperty(cx, constructor, "prototype", prototype, JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(cx, prototype, "constructor", constructor, 0) ||
      !Tag(cx, prototype, "Headers") || !Tag(cx, iterator, "Headers Iterator")) return false;
  struct Definition { const char* name; unsigned length; JSNative native; Operation op; };
  const Definition definitions[] = {
    {"append", 2, Method, Operation::Append}, {"delete", 1, Method, Operation::Delete},
    {"get", 1, Method, Operation::Get}, {"getSetCookie", 0, Method, Operation::Cookies},
    {"has", 1, Method, Operation::Has}, {"set", 2, Method, Operation::Set},
    {"entries", 0, Iterate, Operation::Entries}, {"keys", 0, Iterate, Operation::Keys},
    {"values", 0, Iterate, Operation::Values}, {"forEach", 1, ForEach, Operation::ForEach}
  };
  for (const auto& definition : definitions) {
    JS::RootedObject function(cx, Function(s, definition.native, definition.name, definition.length, definition.op));
    if (!function || !JS_DefineProperty(cx, prototype, definition.name, function, JSPROP_ENUMERATE) ||
        !helpers.Member(function, "Headers", definition.name, "call")) return false;
    if (definition.op == Operation::Entries) {
      JS::RootedId id(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
      if (!JS_DefinePropertyById(cx, prototype, id, function, 0)) return false;
    }
  }
  JS::RootedObject next(cx, Function(s, Next, "next", 0, Operation::Entries));
  return next && JS_DefineProperty(cx, iterator, "next", next, JSPROP_ENUMERATE) &&
      helpers.Member(next, "Headers Iterator", "next", "call") &&
      helpers.Constructor("Headers", constructor) &&
      JS_DefineProperty(cx, s.page, "Headers", constructor, 0);
}
bool Constructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return Error(cx, "Headers requires new");
  if (State* owner = FactoryOwner(args))
    owner->Observe("Headers", "constructor", int(ApiAccess::Construct));
  JS::RootedObject holder(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  JS::RootedObject prototype(cx, &JS::GetReservedSlot(holder, 1).toObject());
  JS::RootedObject target(cx, &args.newTarget().toObject());
  JS::RootedValue desired(cx);
  if (!JS_GetProperty(cx, target, "prototype", &desired)) return false;
  if (desired.isObject()) prototype = &desired.toObject();
  HeaderDraft* raw = nullptr;
  if (!Report(cx, NewDraft(&raw))) return false;
  std::unique_ptr<HeaderDraft, decltype(&DeleteDraft)> draft(raw, DeleteDraft);
  if (!args.get(0).isUndefined() && !ConvertInit(cx, args[0], draft.get())) return false;
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kHeadersClass, prototype));
  if (!object) return false;
  HeaderRecord* record = nullptr;
  auto* arena = static_cast<HeaderArena*>(JS::GetReservedSlot(holder, 0).toPrivate());
  if (!Report(cx, CreateHeaders(arena, draft.get(), &record))) return false;
  JS::SetReservedSlot(object, 0, JS::PrivateValue(record));
  args.rval().setObject(*object);
  return trace.Finish(true);
}
}
