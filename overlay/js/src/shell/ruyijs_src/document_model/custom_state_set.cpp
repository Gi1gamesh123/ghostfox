// CustomStateSet (HTML §4.13.7.7): a setlike<DOMString> whose one state is
// the backing Set in its slot; iterators are the engine's Set iterators, as in
// Firefox. ElementInternals.states hands out one per internals (SameObject).
#include "document_model/state.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/MapAndSet.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
#include <iterator>
#include <string>

namespace ruyijs::document_model {
namespace {
const JSClass kStateSetClass = {"CustomStateSet", JSCLASS_HAS_RESERVED_SLOTS(1)};
enum Op { Add, Delete, Clear, Entries, Keys, Values, ForEach, Has, Size };
const char* const kNames[] = {"add", "delete", "clear", "entries", "keys", "values", "forEach", "has"};
const unsigned kLengths[] = {1, 1, 0, 0, 0, 0, 1, 1};
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const Op op = Op(FunctionSlot(args));
  JSObject* self = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (!self || JS::GetClass(self) != &kStateSetClass) {
    const std::string member = op == Size ? "get size" : kNames[op];
    return TypeError(cx, ("'" + member + "' called on an object that does not implement interface CustomStateSet.").c_str());
  }
  JS::RootedObject set(cx, &JS::GetReservedSlot(self, 0).toObject());
  JS::RootedValue key(cx);
  if ((op == Add || op == Delete || op == Has || op == ForEach) && !argc) {
    const std::string text = std::string("CustomStateSet.") + kNames[op] + ": At least 1 argument required, but only 0 passed";
    return TypeError(cx, text.c_str());
  }
  if (op == Add || op == Delete || op == Has) {
    JS::RootedString text(cx, JS::ToString(cx, args[0]));
    if (!text) return false;
    key.setString(text);
  }
  bool result = false;
  switch (op) {
    case Add: args.rval().setUndefined(); return JS::SetAdd(cx, set, key);
    case Delete: if (!JS::SetDelete(cx, set, key, &result)) return false; args.rval().setBoolean(result); return true;
    case Has: if (!JS::SetHas(cx, set, key, &result)) return false; args.rval().setBoolean(result); return true;
    case Clear: args.rval().setUndefined(); return JS::SetClear(cx, set);
    case Entries: return JS::SetEntries(cx, set, args.rval());
    case Keys: case Values: return JS::SetValues(cx, set, args.rval());
    case Size: args.rval().setNumber(double(JS::SetSize(cx, set))); return true;
    default: break;
  }
  if (!args[0].isObject() || !JS::IsCallable(&args[0].toObject()))
    return TypeError(cx, "CustomStateSet.forEach: Argument 1 is not callable.");
  // The callback's third argument is the CustomStateSet, never the backing Set.
  JS::RootedValue values(cx), item(cx), callback(cx, args[0]), ignored(cx);
  JS::ForOfIterator it(cx);
  if (!JS::SetValues(cx, set, &values) || !it.init(values)) return false;
  JS::RootedValueArray<3> call(cx);
  call[2].setObject(*self);
  for (bool done = false;;) {
    if (!it.next(&item, &done)) return false;
    if (done) break;
    call[0].set(item);
    call[1].set(item);
    if (!JS::Call(cx, args.get(1), callback, call, &ignored)) return false;
  }
  args.rval().setUndefined();
  return true;
}
}  // namespace

JSObject* NewCustomStateSet(State& state) {
  JS::RootedObject prototype(state.cx, state.Find("CustomStateSet")->prototype);
  JS::RootedObject set(state.cx, JS::NewSetObject(state.cx));
  if (!set) return nullptr;
  JSObject* object = JS_NewObjectWithGivenProto(state.cx, &kStateSetClass, prototype);
  if (object) JS::SetReservedSlot(object, 0, JS::ObjectValue(*set));
  return object;
}

bool InstallCustomStateSet(State& state) {
  JSContext* cx = state.cx;
  JS::RootedObject plain(cx, JS::GetRealmObjectPrototype(cx));
  if (!state.DefineInterface("CustomStateSet", plain, Illegal, false)) return false;
  for (int i = 0; i < int(std::size(kNames)); ++i)
    if (!state.Method("CustomStateSet", kNames[i], Member, kLengths[i], i)) return false;
  if (!state.Accessor("CustomStateSet", "size", Member, Size)) return false;
  JS::RootedObject prototype(cx, state.Find("CustomStateSet")->prototype);
  JS::RootedValue values(cx);
  JS::RootedId iterator(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
  return JS_GetProperty(cx, prototype, "values", &values) &&
         JS_DefinePropertyById(cx, prototype, iterator, values, 0);
}
}  // namespace ruyijs::document_model
