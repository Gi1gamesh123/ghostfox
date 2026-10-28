#include "browser_environment/history.h"
#include "browser_environment/location_navigation.h"
#include "browser_environment/string_list.h"
#include "browser_environment/state.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
#include "js/Wrapper.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
#include "js/String.h"
#include <iterator>

namespace ruyijs::browser_environment {
static const JSClass kClasses[] = {
  // Slots 1/2 are the realm-local UserActivation/MediaCapabilities references.
  {"Navigator", JSCLASS_HAS_RESERVED_SLOTS(3)},
  // Slot 1 holds Screen.onmozorientationchange (an EventHandler attribute
  // installed by screen_orientation.cpp); slot 0 is always the State*.
  {"Screen", JSCLASS_HAS_RESERVED_SLOTS(2)},
  {"Location", JSCLASS_HAS_RESERVED_SLOTS(1)},
  {"History", JSCLASS_HAS_RESERVED_SLOTS(1)}
};
static const JSClass kWorkerLocationClass = {"WorkerLocation", JSCLASS_HAS_RESERVED_SLOTS(1)};
static const JSClass kWorkerNavigatorClass = {"WorkerNavigator", JSCLASS_HAS_RESERVED_SLOTS(3)};
const JSClass* ClassFor(Kind kind, bool worker) {
  if (worker && kind == Kind::Navigator) return &kWorkerNavigatorClass;
  return worker && kind == Kind::Location ? &kWorkerLocationClass : &kClasses[int32_t(kind)];
}
State* ReceiverState(const JS::CallArgs& args, const JSClass* clasp) {
  if (!args.thisv().isObject()) return nullptr;
  JSObject* self = js::CheckedUnwrapStatic(&args.thisv().toObject());
  if (!self || JS::GetClass(self) != clasp) return nullptr;
  const JS::Value& owner = JS::GetReservedSlot(self, 0);
  return owner.isUndefined() ? nullptr : static_cast<State*>(owner.toPrivate());
}
static bool Constructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return Error(cx, "Illegal constructor.");
}
JSObject* State::Function(JSNative callback, const char* name, unsigned length,
                           int32_t slot, unsigned flags) {
  JS::RootedFunction function(cx,
      js::NewFunctionWithReserved(cx, callback, length, flags, name));
  if (!function) return nullptr;
  JS::RootedObject object(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(this));
  js::SetFunctionNativeReserved(object, 1, JS::Int32Value(slot));
  if (!functions.get().append(object)) { JS_ReportOutOfMemory(cx); return nullptr; }
  return object;
}
bool State::Accessor(JS::HandleObject target, const char* name, int32_t slot,
                       unsigned attrs, JSNative getter, JSNative setter) {
  std::string getName = std::string("get ") + name;
  JS::RootedObject get(cx, Function(getter, getName.c_str(), 0, slot));
  JS::RootedObject set(cx);
  if (setter) {
    std::string setName = std::string("set ") + name;
    set = Function(setter, setName.c_str(), 1, slot);
    if (!set) return false;
  }
  return get && JS_DefineProperty(cx, target, name, get, set, attrs);
}
bool State::Instance(Kind kind, JS::HandleObject parent) {
  const JSClass* klass = ClassFor(kind, worker);
  const bool windowLocation = kind == Kind::Location && !worker;
  JS::RootedObject prototype(cx, JS_NewPlainObject(cx));
  JS::RootedObject constructor(cx, Function(Constructor, klass->name, 0, 0,
                                            JSFUN_CONSTRUCTOR));
  if (!prototype || !constructor || !JS_SetPrototype(cx, prototype, parent)) return false;
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, klass, prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, 0, JS::PrivateValue(this));
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, klass->name));
  JS::RootedId tagId(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx,
                                                JS::SymbolCode::toStringTag)));
  if (!tag || !JS_DefineProperty(cx, constructor, "prototype", prototype,
                                  JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(cx, prototype, "constructor", constructor, 0) ||
      !JS_DefinePropertyById(cx, prototype, tagId, tag, JSPROP_READONLY)) return false;
  const Field historyField{"length", ValueType::Integer};
  const Field* fields = kind == Kind::Navigator ? kNavigatorFields :
      kind == Kind::Screen ? kScreenFields : kind == Kind::Location ? kLocationFields : &historyField;
  size_t count = kind == Kind::Navigator ? std::size(kNavigatorFields) :
      kind == Kind::Screen ? std::size(kScreenFields) :
      kind == Kind::Location ? std::size(kLocationFields) : 1;
  if (kind == Kind::Navigator) navigatorPrototype = prototype;
  JS::RootedObject target(cx, windowLocation ? object : prototype);
  unsigned attrs = JSPROP_ENUMERATE | (windowLocation ? JSPROP_PERMANENT : 0);
  for (size_t index = 0; index < count; ++index) {
    if (worker && kind == Kind::Navigator && !fields[index].worker) continue;
    // Location is the one unforgeable interface: href and the six URL
    // components are writable, origin is not.
    JSNative setter = windowLocation && index < 8 ? LocationSetter : nullptr;
    if (!Accessor(target, fields[index].name, int32_t(kind) * 32 + int32_t(index),
                   attrs, Getter, setter)) return false;
  }
  if (kind == Kind::History && !InstallHistoryMembers(*this, prototype)) return false;
  if (kind == Kind::Location) {
    JS::RootedObject method(cx, Function(LocationString, "toString", 0, 0));
    if (!method || !JS_DefineProperty(cx, target, "toString", method,
          JSPROP_ENUMERATE | (windowLocation ? JSPROP_PERMANENT | JSPROP_READONLY : 0))) return false;
    if (windowLocation && (!InstallLocationMembers(*this, object) ||
        !InstallAncestorOrigins(*this, object))) return false;
  }
  while (instances.get().length() <= size_t(kind)) {
    if (!instances.get().append(nullptr)) { JS_ReportOutOfMemory(cx); return false; }
  }
  instances.get()[size_t(kind)] = object;
  return JS_DefineProperty(cx, page, klass->name, constructor, 0) &&
         helpers->Interface(klass->name, target) && helpers->Object(klass->name, prototype) &&
         helpers->Constructor(klass->name, constructor);
}
}
