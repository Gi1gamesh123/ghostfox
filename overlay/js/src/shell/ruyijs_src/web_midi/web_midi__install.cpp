// The one production installer of MIDIAccess, MIDIInputMap, MIDIOutputMap,
// MIDIPort, MIDIInput and MIDIOutput (dom/webidl/MIDI*.webidl) and of
// Navigator.requestMIDIAccess. The two MIDI events stay with Interaction's
// derived event table. No device service exists: requestMIDIAccess validates
// its dictionary and returns a promise that stays pending, as Firefox does
// while its permission prompt is open.
#include "web_midi/realm.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "runtime/secure_slot.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
#include <string>

namespace ruyijs::web_midi {
namespace {
const JSClass kAccessClass = {"MIDIAccess", 0};
const JSClass kInputMapClass = {"MIDIInputMap", 0};
const JSClass kOutputMapClass = {"MIDIOutputMap", 0};
const JSClass kPortClass = {"MIDIPort", 0};
const JSClass kInputClass = {"MIDIInput", 0};
const JSClass kOutputClass = {"MIDIOutput", 0};
}  // namespace

struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events,
        browser_environment::Environment& environment)
      : cx(cx), kit(cx, page, helpers, this), events(events), environment(environment), access(cx),
        inputMap(cx), outputMap(cx), port(cx), input(cx), output(cx), secure(cx) {}
  JSContext* cx;
  WebKit kit;
  interaction::Realm& events;
  browser_environment::Environment& environment;
  WebFace access, inputMap, outputMap, port, input, output;
  SecureSlots secure;
};

namespace {
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
// Every member of an interface that never has an instance in this runtime: the
// receiver check is all it can do, with the donor binding's message.
struct Member { const char* iface; const char* name; bool getter; unsigned length; };
const Member kMembers[] = {
    {"MIDIAccess", "inputs", true, 0}, {"MIDIAccess", "outputs", true, 0},
    {"MIDIAccess", "onstatechange", true, 0}, {"MIDIAccess", "sysexEnabled", true, 0},
    {"MIDIPort", "open", false, 0}, {"MIDIPort", "close", false, 0},
    {"MIDIPort", "id", true, 0}, {"MIDIPort", "manufacturer", true, 0},
    {"MIDIPort", "name", true, 0}, {"MIDIPort", "version", true, 0},
    {"MIDIPort", "type", true, 0}, {"MIDIPort", "state", true, 0},
    {"MIDIPort", "connection", true, 0}, {"MIDIPort", "onstatechange", true, 0},
    {"MIDIInput", "onmidimessage", true, 0},
    {"MIDIOutput", "send", false, 1}, {"MIDIOutput", "clear", false, 0}};
// Members with a setter (EventHandler attributes).
bool HasSetter(const char* name) { return std::string(name).rfind("on", 0) == 0; }
const char* const kMapMethods[] = {"entries", "keys", "values", "forEach", "has", "get"};
const unsigned kMapLengths[] = {0, 0, 0, 1, 1, 1};

bool Incompatible(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedString name(cx);
  JS::RootedFunction function(cx, JS_GetObjectFunction(&args.callee()));
  std::string member;
  if (function && JS_GetFunctionDisplayId(cx, function, &name) && name) {
    JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, name);
    if (bytes) member = bytes.get();
  }
  const auto* iface = static_cast<const char*>(js::GetFunctionNativeReserved(&args.callee(), 1).toPrivate());
  return WebKit::Fail(cx, ("'" + member + "' called on an object that does not implement interface " +
                           iface + ".").c_str());
}
JSObject* Native(State& s, const char* iface, const char* name, unsigned length) {
  JSFunction* function = js::NewFunctionWithReserved(s.cx, Incompatible, length, 0, name);
  if (!function) return nullptr;
  JSObject* object = JS_GetFunctionObject(function);
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(&s));
  js::SetFunctionNativeReserved(object, 1, JS::PrivateValue(const_cast<char*>(iface)));
  return object;
}
WebFace& Face(State& s, const std::string& iface) {
  return iface == "MIDIAccess" ? s.access : iface == "MIDIPort" ? s.port
       : iface == "MIDIInput" ? s.input : s.output;
}
bool InstallMembers(State& s) {
  JSContext* cx = s.cx;
  for (const Member& m : kMembers) {
    JS::RootedObject prototype(cx, Face(s, m.iface).prototype);
    if (!m.getter) {
      JS::RootedObject method(cx, Native(s, m.iface, m.name, m.length));
      if (!method || !JS_DefineProperty(cx, prototype, m.name, method, JSPROP_ENUMERATE)) return false;
      continue;
    }
    const std::string get = std::string("get ") + m.name, set = std::string("set ") + m.name;
    JS::RootedObject getter(cx, Native(s, m.iface, get.c_str(), 0)), setter(cx);
    if (HasSetter(m.name)) setter = Native(s, m.iface, set.c_str(), 1);
    if (!getter || (HasSetter(m.name) && !setter) ||
        !JS_DefineProperty(cx, prototype, m.name, getter, setter, JSPROP_ENUMERATE)) return false;
  }
  // maplike<DOMString, MIDIInput/MIDIOutput>, read-only.
  for (int which = 0; which < 2; ++which) {
    WebFace& face = which ? s.outputMap : s.inputMap;
    const char* iface = which ? "MIDIOutputMap" : "MIDIInputMap";
    JS::RootedObject prototype(cx, face.prototype), entries(cx);
    for (size_t i = 0; i < std::size(kMapMethods); ++i) {
      JS::RootedObject method(cx, Native(s, iface, kMapMethods[i], kMapLengths[i]));
      if (!method || !JS_DefineProperty(cx, prototype, kMapMethods[i], method, JSPROP_ENUMERATE)) return false;
      if (i == 0) entries = method;
    }
    JS::RootedObject size(cx, Native(s, iface, "get size", 0)), none(cx);
    JS::RootedId iterator(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
    JS::RootedValue entriesValue(cx, JS::ObjectValue(*entries));
    if (!size || !JS_DefineProperty(cx, prototype, "size", size, none, JSPROP_ENUMERATE) ||
        !JS_DefinePropertyById(cx, prototype, iterator, entriesValue, 0)) return false;
  }
  return true;
}
// requestMIDIAccess(optional MIDIOptions options = {}): dictionary conversion,
// then a promise that waits on a permission prompt this runtime never shows.
bool RequestMIDIAccess(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::HandleValue options = args.get(0);
  if (!options.isNullOrUndefined()) {
    if (!options.isObject()) {
      (void)WebKit::Fail(cx, "Navigator.requestMIDIAccess: Argument 1 can't be converted to a dictionary.");
      JS::RootedValue error(cx);
      if (!JS_GetPendingException(cx, &error)) return false;
      JS_ClearPendingException(cx);
      JSObject* rejected = JS::CallOriginalPromiseReject(cx, error);
      if (!rejected) return false;
      args.rval().setObject(*rejected);
      return true;
    }
    JS::RootedObject object(cx, &options.toObject());
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, object, "software", &value) || !JS_GetProperty(cx, object, "sysex", &value))
      return false;
  }
  JSObject* promise = JS::NewPromiseObject(cx, nullptr);
  if (!promise) return false;
  args.rval().setObject(*promise);
  return true;
}
}  // namespace

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events,
             browser_environment::Environment& environment)
    : state_(std::make_unique<State>(cx, page, helpers, events, environment)) {}
Realm::~Realm() = default;

bool Realm::Install() {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedObject objectPrototype(cx, JS::GetRealmObjectPrototype(cx)), targetPrototype(cx, s.events.prototype());
  if (!objectPrototype || !targetPrototype ||
      !s.kit.Define("MIDIAccess", &kAccessClass, targetPrototype, Illegal, &s.access) ||
      !s.kit.Define("MIDIInputMap", &kInputMapClass, objectPrototype, Illegal, &s.inputMap) ||
      !s.kit.Define("MIDIOutputMap", &kOutputMapClass, objectPrototype, Illegal, &s.outputMap) ||
      !s.kit.Define("MIDIPort", &kPortClass, targetPrototype, Illegal, &s.port)) return false;
  JS::RootedObject portPrototype(cx, s.port.prototype);
  if (!s.kit.Define("MIDIInput", &kInputClass, portPrototype, Illegal, &s.input) ||
      !s.kit.Define("MIDIOutput", &kOutputClass, portPrototype, Illegal, &s.output)) return false;
  // MIDIInput/MIDIOutput constructors inherit MIDIPort's interface object.
  JS::RootedObject portCtor(cx, s.port.constructor), inputCtor(cx, s.input.constructor),
      outputCtor(cx, s.output.constructor);
  if (!JS_SetPrototype(cx, inputCtor, portCtor) || !JS_SetPrototype(cx, outputCtor, portCtor) ||
      !InstallMembers(s)) return false;
  JS::RootedObject navigatorPrototype(cx, s.environment.navigatorPrototype());
  JS::RootedObject method(cx, s.kit.Function(RequestMIDIAccess, "requestMIDIAccess", 0, 0));
  if (!navigatorPrototype || !method ||
      !JS_DefineProperty(cx, navigatorPrototype, "requestMIDIAccess", method, JSPROP_ENUMERATE) ||
      !s.kit.helpers->Member(method, "Navigator", "requestMIDIAccess", "call") ||
      !s.secure.Add(navigatorPrototype, "requestMIDIAccess")) return false;
  for (const char* name : {"MIDIAccess", "MIDIInputMap", "MIDIOutputMap", "MIDIPort", "MIDIInput", "MIDIOutput"})
    if (!s.secure.Add(s.kit.page, name)) return false;
  return true;
}

bool Realm::ConfigureExposure() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedValue secure(s.cx);
  return JS_GetProperty(s.cx, s.kit.page, "isSecureContext", &secure) && s.secure.Apply(secure.isTrue());
}
}  // namespace ruyijs::web_midi
