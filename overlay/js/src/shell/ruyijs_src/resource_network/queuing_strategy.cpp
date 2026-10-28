// Streams strategy values and realm-local factories belong to ResourceNetwork.
#include "resource_network/state.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_reference.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::resource_network {
namespace {
const JSClass kByteLengthClass = {"ByteLengthQueuingStrategy", JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kCountClass = {"CountQueuingStrategy", JSCLASS_HAS_RESERVED_SLOTS(2)};
// Only retained factory prototypes, never another strategy state source.
const JSClass kPrototypePairClass = {"Resource strategy prototypes", JSCLASS_HAS_RESERVED_SLOTS(2)};

bool ByteLengthSize(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject chunk(cx, JS::ToObject(cx, args.get(0)));
  return chunk && JS_GetProperty(cx, chunk, "byteLength", args.rval());
}
bool CountSize(JSContext*, unsigned argc, JS::Value* vp) {
  JS::CallArgsFromVp(argc, vp).rval().setInt32(1);
  return true;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int slot = WebKit::Slot(args);
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!object || JS::GetClass(object) != (slot < 2 ? &kByteLengthClass : &kCountClass))
    return Error(cx, "Queuing strategy getter called on incompatible receiver");
  static_cast<State*>(WebKit::Owner(args))->Observe(
      slot < 2 ? "ByteLengthQueuingStrategy" : "CountQueuingStrategy",
      slot % 2 ? "size" : "highWaterMark", int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(object, slot % 2));
  return JS_WrapValue(cx, args.rval());
}
bool DesiredPrototype(JSContext* cx, const JS::CallArgs& args, unsigned kind,
                      JS::MutableHandleObject result) {
  JS::RootedObject target(cx, &args.newTarget().toObject());
  JS::RootedValue prototype(cx);
  if (!JS_GetProperty(cx, target, "prototype", &prototype)) return false;
  if (prototype.isObject()) { result.set(&prototype.toObject()); return true; }
  JS::Realm* realm = JS::GetFunctionRealm(cx, target);
  if (!realm) return false;
  JS::RootedObject global(cx, JS::GetRealmGlobalOrNull(realm));
  JS::RootedValue retained(cx);
  if (!ReadPageReference(global, PageReference::ResourceStrategyPrototypes, &retained) ||
      !retained.isObject() || JS::GetClass(&retained.toObject()) != &kPrototypePairClass)
    return Error(cx, "Queuing strategy target realm is not installed");
  result.set(&JS::GetReservedSlot(&retained.toObject(), kind).toObject());
  return JS_WrapObject(cx, result);
}
bool Construct(JSContext* cx, unsigned argc, JS::Value* vp, unsigned kind) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return Error(cx, "Queuing strategy requires new");
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject prototype(cx);
  // WebIDL chooses the prototype before converting the required dictionary.
  if (!DesiredPrototype(cx, args, kind, &prototype)) return false;
  if (!argc || !args[0].isObject()) return Error(cx, "Queuing strategy requires an init dictionary");
  JS::RootedObject init(cx, &args[0].toObject());
  JS::RootedValue input(cx);
  if (!JS_GetProperty(cx, init, "highWaterMark", &input)) return false;
  if (input.isUndefined()) return Error(cx, "Queuing strategy requires highWaterMark");
  double highWaterMark;
  if (!JS::ToNumber(cx, input, &highWaterMark)) return false;
  JS::RootedObject object(cx, state.kit.New(kind ? &kCountClass : &kByteLengthClass, prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, 0, JS::DoubleValue(highWaterMark));
  JS::SetReservedSlot(object, 1, JS::ObjectValue(*(kind ? state.countSize : state.byteLengthSize)));
  state.Observe(kind ? "CountQueuingStrategy" : "ByteLengthQueuingStrategy",
                "constructor", int(ApiAccess::Construct));
  args.rval().setObject(*object);
  return trace.Finish(true);
}
bool ByteLengthConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  return Construct(cx, argc, vp, 0);
}
bool CountConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  return Construct(cx, argc, vp, 1);
}
bool InstallStrategy(State& state, unsigned kind, JS::HandleObject retained) {
  JSContext* cx = state.cx;
  WebFace& face = kind ? state.countStrategy : state.byteLengthStrategy;
  const char* name = kind ? "CountQueuingStrategy" : "ByteLengthQueuingStrategy";
  JS::RootedObject parent(cx, JS::GetRealmObjectPrototype(cx));
  JS::RootedFunction size(cx, JS_NewFunction(cx, kind ? CountSize : ByteLengthSize,
                                            kind ? 0 : 1, 0, "size"));
  if (!parent || !size) return false;
  (kind ? state.countSize : state.byteLengthSize) = JS_GetFunctionObject(size);
  if (!state.kit.Define(name, kind ? &kCountClass : &kByteLengthClass, parent,
                       kind ? CountConstructor : ByteLengthConstructor, &face, 1) ||
      !state.kit.Accessor(face, name, "highWaterMark", Get, nullptr, int32_t(kind * 2)) ||
      !state.kit.Accessor(face, name, "size", Get, nullptr, int32_t(kind * 2 + 1))) return false;
  JS::SetReservedSlot(retained, kind, JS::ObjectValue(*face.prototype));
  return true;
}
}  // namespace

bool InstallQueuingStrategies(State& state, PageHelpers&) {
  JS::RootedObject retained(state.cx,
      JS_NewObjectWithGivenProto(state.cx, &kPrototypePairClass, nullptr));
  if (!retained || !InstallStrategy(state, 0, retained) || !InstallStrategy(state, 1, retained))
    return false;
  JS::RootedValue value(state.cx, JS::ObjectValue(*retained));
  return WritePageReference(state.page, PageReference::ResourceStrategyPrototypes, value);
}
}  // namespace ruyijs::resource_network
