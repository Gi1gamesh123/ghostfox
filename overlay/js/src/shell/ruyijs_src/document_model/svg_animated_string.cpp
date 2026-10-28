// Class-name view: the original element attribute is the sole data source.
#include "document_model/state.h"
#include "runtime/weak_object.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
void Finalize(JS::GCContext*, JSObject* object) {
  auto* link = static_cast<ReflectedLink*>(JS::GetReservedSlot(object, 2).toPrivate());
  if (link && link->animatedClass == object) link->animatedClass = nullptr;
}
void Trace(JSTracer*, JSObject* object) {
  auto* link = static_cast<ReflectedLink*>(JS::GetReservedSlot(object, 2).toPrivate());
  if (link) link->animatedClass = object;
}
const JSClassOps kOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, Finalize, nullptr, nullptr, Trace};
const JSClass kAnimatedStringClass = {"SVGAnimatedString", JSCLASS_HAS_RESERVED_SLOTS(3) |
    JSCLASS_FOREGROUND_FINALIZE, &kOps};
Wrapper* Receiver(JSContext* cx, JS::HandleValue value) {
  JS::RootedObject object(cx, value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (!object || JS::GetClass(object) != &kAnimatedStringClass) {
    TypeError(cx, "SVGAnimatedString receiver is incompatible"); return nullptr;
  }
  JS::RootedValue node(cx, JS::GetReservedSlot(object, 0));
  return ElementReceiver(cx, node);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* entry = Receiver(cx, args.thisv());
  if (!entry) return false;
  std::optional<DomString> value;
  if (!FunctionState(args)->Check(entry->owner->model.GetAttribute(entry->key, u"class", &value))) return false;
  return Value(cx, value ? DomStringView(*value) : DomStringView(), args.rval());
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* entry = Receiver(cx, args.thisv());
  if (!entry) return false;
  DomString value;
  if (!String(cx, args.get(0), &value) ||
      !FunctionState(args)->Check(entry->owner->model.SetAttribute(entry->key, u"class", value))) return false;
  args.rval().setUndefined(); return true;
}
}
bool AnimatedClassObject(Wrapper& entry, JS::MutableHandleValue result) {
  State& owner = *entry.owner;
  State& factory = entry.factory ? *entry.factory : owner;
  auto* link = owner.Reflected(entry.key);
  if (!link) return false;
  JS::RootedObject object(owner.cx, WeakLive(link->animatedClass));
  if (!object) {
    JSAutoRealm realm(owner.cx, factory.page);
    JS::RootedObject node(owner.cx, entry.object), prototype(owner.cx, factory.Find("SVGAnimatedString")->prototype);
    if (!JS_WrapObject(owner.cx, &node)) return false;
    object = JS_NewObjectWithGivenProto(owner.cx, &kAnimatedStringClass, prototype);
    if (!object) return false;
    JS::SetReservedSlot(object, 0, JS::ObjectValue(*node));
    JS::SetReservedSlot(object, 1, JS::PrivateValue(&factory));
    JS::SetReservedSlot(object, 2, JS::PrivateValue(link));
    link->animatedClass = object;
  }
  if (!JS_WrapObject(owner.cx, &object)) return false;
  result.setObject(*object); return true;
}
bool InstallAnimatedString(State& state) {
  JS::RootedObject parent(state.cx, JS::GetRealmObjectPrototype(state.cx));
  return state.DefineInterface("SVGAnimatedString", parent, Illegal) &&
      state.Accessor("SVGAnimatedString", "baseVal", Get, 0, Set) &&
      state.Accessor("SVGAnimatedString", "animVal", Get, 0);
}
}
