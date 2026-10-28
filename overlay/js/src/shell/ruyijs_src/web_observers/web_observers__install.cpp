// MutationObserver / MutationRecord installation plus the realm-local
// interface scaffolding the T11c observers share. The interface shape follows
// the Firefox donor: MutationRecord's nine members are [Constant] reads of the
// instance's reserved slots; MutationObserver's three members brand their
// receiver. The donor generated bindings define a prototype's methods, then
// attributes, then @@toStringTag, and link "constructor" last
// (BindingUtils.cpp::CreateInterfacePrototypeObject / JS_LinkConstructorAndPrototype);
// NewInterface/LinkInterface keep that split so every T11c installer can put
// its members in donor order.
#include "web_observers/realm.h"
#include "web_observers/internal.h"
#include "runtime/web_kit.h"
#include "document_model/realm.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Symbol.h"
#include "jsfriendapi.h"
#include <string>

namespace ruyijs::web_observers {
const JSClass kObserverClass = {"MutationObserver", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kRecordClass = {"MutationRecord", JSCLASS_HAS_RESERVED_SLOTS(9)};

JSObject* Function(State& state, JSNative native, const char* name, unsigned length,
                   int32_t slot, unsigned flags) {
  JS::RootedFunction function(
      state.cx, js::NewFunctionWithReserved(state.cx, native, length, flags, name));
  if (!function) return nullptr;
  JS::RootedObject object(state.cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(&state));
  js::SetFunctionNativeReserved(object, 1, JS::Int32Value(slot));
  return object;
}

bool Tag(JSContext* cx, JS::HandleObject object, const char* text) {
  JS::RootedId id(cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  JS::RootedString value(cx, JS_NewStringCopyZ(cx, text));
  return value && JS_DefinePropertyById(cx, object, id, value, JSPROP_READONLY);
}

bool NewInterface(State& state, const char* name, unsigned length, JSNative constructor,
                  JS::MutableHandleObject prototype, JS::MutableHandleObject object) {
  JSContext* cx = state.cx;
  JS::RootedObject proto(cx, JS_NewPlainObject(cx));
  JS::RootedObject ctor(cx, Function(state, constructor, name, length, 0, JSFUN_CONSTRUCTOR));
  if (!proto || !ctor ||
      !JS_DefineProperty(cx, ctor, "prototype", proto, JSPROP_READONLY | JSPROP_PERMANENT) ||
      !Tag(cx, proto, name) ||
      !state.helpers->Constructor(name, ctor) ||
      !state.helpers->Interface(name, proto)) return false;
  prototype.set(proto);
  object.set(ctor);
  return true;
}

bool LinkInterface(State& state, JS::HandleObject prototype, JS::HandleObject object) {
  JS::RootedObject proto(state.cx, prototype);
  JS::RootedObject ctor(state.cx, object);
  return JS_DefineProperty(state.cx, proto, "constructor", ctor, 0);
}

bool Getter(State& state, const char* iface, JS::HandleObject prototype,
            const char* name, JSNative native, int32_t slot) {
  const std::string getterName = std::string("get ") + name;
  JS::RootedObject getter(state.cx, Function(state, native, getterName.c_str(), 0, slot));
  JS::RootedObject setter(state.cx);
  return getter &&
         JS_DefineProperty(state.cx, prototype, name, getter, setter, JSPROP_ENUMERATE) &&
         state.helpers->Accessor(iface, prototype, name);
}

bool Method(State& state, const char* iface, JS::HandleObject prototype,
            const char* name, JSNative native, unsigned length) {
  JS::RootedObject method(state.cx, Function(state, native, name, length, 0));
  return method &&
         JS_DefineProperty(state.cx, prototype, name, method, JSPROP_ENUMERATE) &&
         state.helpers->Member(method, iface, name, "call");
}
namespace {
// One [Constant] MutationRecord member: a getter on the prototype that reads
// the instance's own slot. A record always carries its values.
bool Member(State& state, JS::HandleObject prototype, const char* name, int32_t slot) {
  return Getter(state, "MutationRecord", prototype, name, RecordMember, slot);
}

// The realm's own interface prototype behind a global interface object. The
// (Element or Document) conversion of IntersectionObserver needs the installed
// prototypes; they are read, never created here.
bool RealmPrototype(State& state, const char* name, JS::PersistentRootedObject* out) {
  JSContext* cx = state.cx;
  JS::RootedValue interface(cx);
  JS::RootedObject object(cx);
  JS::RootedValue prototype(cx);
  if (!JS_GetProperty(cx, state.page, name, &interface) || !interface.isObject()) {
    JS_ReportErrorASCII(cx, "observer install requires the element interfaces");
    return false;
  }
  object = &interface.toObject();
  if (!JS_GetProperty(cx, object, "prototype", &prototype) || !prototype.isObject()) {
    JS_ReportErrorASCII(cx, "observer install requires the element interfaces");
    return false;
  }
  *out = &prototype.toObject();
  return true;
}
}  // namespace

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             document_model::Realm& document)
    : state_(std::make_unique<State>(cx, page, helpers, &document)) {}
Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers)
    : state_(std::make_unique<State>(cx, page, helpers, nullptr)) {}
Realm::~Realm() = default;
// Worker realms: ReportingObserver and ReportBody only (Exposed=(Window,Worker)).
bool Realm::InstallWorker() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.page);
  return InstallReporting(s);
}
bool ThrowType(State* state, const char* message) {
  if (state && state->document) return state->document->ThrowTypeError(message);
  JSContext* cx = state ? state->cx : nullptr;
  return cx ? WebKit::Fail(cx, message) : false;
}
void Realm::SetMicrotaskPort(MicrotaskPort port, void* context) {
  state_->microtask = port;
  state_->microtaskContext = context;
}
void Realm::SetClockPort(ClockPort port, void* context) {
  state_->clock = port;
  state_->clockContext = context;
}
void Realm::CopyPortsFrom(const Realm& other) {
  state_->JoinMutationGroup(other.state_->mutationGroup);
  state_->microtask = other.state_->microtask;
  state_->microtaskContext = other.state_->microtaskContext;
  state_->clock = other.state_->clock;
  state_->clockContext = other.state_->clockContext;
}

bool Realm::Install() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.page);
  JS::RootedObject recordPrototype(s.cx, JS_NewPlainObject(s.cx));
  JS::RootedObject recordConstructor(
      s.cx, Function(s, RecordConstructor, "MutationRecord", 0, 0, JSFUN_CONSTRUCTOR));
  JS::RootedObject observerPrototype(s.cx, JS_NewPlainObject(s.cx));
  JS::RootedObject observerConstructor(
      s.cx, Function(s, ObserverConstructor, "MutationObserver", 1, 0, JSFUN_CONSTRUCTOR));
  JS::RootedObject observe(s.cx, Function(s, Observe, "observe", 2, 0));
  JS::RootedObject disconnect(s.cx, Function(s, Disconnect, "disconnect", 0, 0));
  JS::RootedObject take(s.cx, Function(s, TakeRecords, "takeRecords", 0, 0));
  JS::RootedObject deliver(s.cx, Function(s, Deliver, "deliver", 0, 0));
  if (!recordPrototype || !recordConstructor || !observerPrototype || !observerConstructor ||
      !observe || !disconnect || !take || !deliver) return false;
  // The factory and the getter path both read these two off the realm state,
  // so the installing realm's own prototypes are what every later object uses.
  s.observerPrototype = observerPrototype.get();
  s.recordPrototype = recordPrototype.get();
  static const char* const fields[] = {"type", "target", "addedNodes", "removedNodes",
                                       "previousSibling", "nextSibling", "attributeName",
                                       "attributeNamespace", "oldValue"};
  for (int32_t slot = 0; slot < 9; ++slot)
    if (!Member(s, recordPrototype, fields[slot], slot)) return false;
  if (!JS_DefineProperty(s.cx, recordConstructor, "prototype", recordPrototype,
                         JSPROP_READONLY | JSPROP_PERMANENT) ||
      !LinkInterface(s, recordPrototype, recordConstructor) ||
      !Tag(s.cx, recordPrototype, "MutationRecord") ||
      !JS_DefineProperty(s.cx, observerConstructor, "prototype", observerPrototype,
                         JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(s.cx, observerPrototype, "constructor", observerConstructor, 0) ||
      !Tag(s.cx, observerPrototype, "MutationObserver") ||
      !JS_DefineProperty(s.cx, observerPrototype, "observe", observe, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(s.cx, observerPrototype, "disconnect", disconnect, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(s.cx, observerPrototype, "takeRecords", take, JSPROP_ENUMERATE) ||
      !s.helpers->Member(observe, "MutationObserver", "observe", "call") ||
      !s.helpers->Member(disconnect, "MutationObserver", "disconnect", "call") ||
      !s.helpers->Member(take, "MutationObserver", "takeRecords", "call") ||
      !s.helpers->Constructor("MutationObserver", observerConstructor) ||
      !s.helpers->Constructor("MutationRecord", recordConstructor) ||
      !s.helpers->Interface("MutationObserver", observerPrototype) ||
      !s.helpers->Interface("MutationRecord", recordPrototype) ||
      !JS_DefineProperty(s.cx, s.page, "MutationObserver", observerConstructor, 0) ||
      !JS_DefineProperty(s.cx, s.page, "MutationRecord", recordConstructor, 0)) return false;
  s.deliver = deliver.get();
  if (!RealmPrototype(s, "Element", &s.elementPrototype) ||
      !RealmPrototype(s, "Document", &s.documentPrototype)) return false;
  return InstallIntersection(s) && InstallResize(s) && InstallReporting(s) && InstallReportBodies(s);
}

bool State::IsElement(JS::HandleValue value) const {
  if (!value.isObject()) return false;
  JS::RootedObject object(cx, &value.toObject());
  JS::RootedObject target(cx, elementPrototype);
  while (object) {
    if (object == target) return true;
    JS::RootedObject proto(cx);
    if (!JS_GetPrototype(cx, object, &proto)) return false;
    object = proto;
  }
  return false;
}

bool State::IsDocument(JS::HandleValue value) const {
  if (!value.isObject()) return false;
  JS::RootedObject object(cx, &value.toObject());
  JS::RootedObject target(cx, documentPrototype);
  while (object) {
    if (object == target) return true;
    JS::RootedObject proto(cx);
    if (!JS_GetPrototype(cx, object, &proto)) return false;
    object = proto;
  }
  return false;
}
}  // namespace ruyijs::web_observers
