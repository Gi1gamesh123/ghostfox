// The HTML "PDF viewing support" surface, shaped from the donor's hard-coded
// plugin set. One installer owns the four interfaces, their four global
// constructors, the two Navigator accessors and every instance the getters
// return. Gecko backs PluginArray/MimeTypeArray with proxies; here the
// array-likes are plain objects whose own index properties are enumerable and
// whose own named properties are not, which is the observable shape of the
// donor binding's [LegacyUnenumerableNamedProperties] getters (see
// docs/DEVIATIONS.md D46).
#include "browser_environment/plugins.h"
#include "browser_environment/schema.h"
#include "browser_environment/state.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <iterator>

namespace ruyijs::browser_environment {
namespace {
// donor dom/base/nsPluginArray.cpp:25-30 and dom/base/nsMimeTypeArray.h: the
// five plugin names, the two shared MIME types, and the two constants every
// entry reports. pdfjs.disabled is false in the 157 default build, so the
// arrays are never empty (nsPluginArray::ForceNoPlugins).
const char* const kPluginNames[] = {"PDF Viewer", "Chrome PDF Viewer",
                                    "Chromium PDF Viewer",
                                    "Microsoft Edge PDF Viewer",
                                    "WebKit built-in PDF"};
const char* const kMimeTypeNames[] = {"application/pdf", "text/pdf"};
const char kPluginDescription[] = "Portable Document Format";
const char kPluginFilename[] = "internal-pdf-viewer";
const char kMimeDescription[] = "Portable Document Format";
const char kMimeSuffixes[] = "pdf";
constexpr int32_t kPluginCount = int32_t(std::size(kPluginNames));
constexpr int32_t kMimeCount = int32_t(std::size(kMimeTypeNames));

const JSClass kPluginArrayClass = {"PluginArray", JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kPluginClass = {"Plugin", JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kMimeTypeArrayClass = {"MimeTypeArray", JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kMimeTypeClass = {"MimeType", JSCLASS_HAS_RESERVED_SLOTS(2)};

enum Member : int32_t {
  kPluginArrayLength = 400, kPluginArrayItem, kPluginArrayNamedItem, kPluginArrayRefresh,
  kPluginLength = 410, kPluginItem, kPluginNamedItem, kPluginName, kPluginDescriptionSlot,
  kPluginFilenameSlot, kMimeArrayLength = 420, kMimeArrayItem, kMimeArrayNamedItem,
  kMimeTypeSlot = 430, kMimeTypeDescription, kMimeTypeSuffixes, kMimeTypeEnabledPlugin,
  kNavigatorPlugins = 440, kNavigatorMimeTypes
};
// One table names every member once: the installer reads it, the natives
// validate their receiver against its class, and the debug monitor receives its
// static interface/member name. klass is null for the two Navigator accessors,
// which are validated against the Navigator instance instead.
struct MemberSpec {
  const char* iface; const char* name; int32_t slot; const JSClass* klass; bool method;
};
const MemberSpec kMembers[] = {
  // Firefox 158 prototype key order per interface.
  {"PluginArray", "refresh", kPluginArrayRefresh, &kPluginArrayClass, true},
  {"PluginArray", "item", kPluginArrayItem, &kPluginArrayClass, true},
  {"PluginArray", "namedItem", kPluginArrayNamedItem, &kPluginArrayClass, true},
  {"PluginArray", "length", kPluginArrayLength, &kPluginArrayClass, false},
  {"Plugin", "item", kPluginItem, &kPluginClass, true},
  {"Plugin", "namedItem", kPluginNamedItem, &kPluginClass, true},
  {"Plugin", "description", kPluginDescriptionSlot, &kPluginClass, false},
  {"Plugin", "filename", kPluginFilenameSlot, &kPluginClass, false},
  {"Plugin", "name", kPluginName, &kPluginClass, false},
  {"Plugin", "length", kPluginLength, &kPluginClass, false},
  {"MimeTypeArray", "item", kMimeArrayItem, &kMimeTypeArrayClass, true},
  {"MimeTypeArray", "namedItem", kMimeArrayNamedItem, &kMimeTypeArrayClass, true},
  {"MimeTypeArray", "length", kMimeArrayLength, &kMimeTypeArrayClass, false},
  {"MimeType", "type", kMimeTypeSlot, &kMimeTypeClass, false},
  {"MimeType", "description", kMimeTypeDescription, &kMimeTypeClass, false},
  {"MimeType", "suffixes", kMimeTypeSuffixes, &kMimeTypeClass, false},
  {"MimeType", "enabledPlugin", kMimeTypeEnabledPlugin, &kMimeTypeClass, false},
  {"Navigator", "plugins", kNavigatorPlugins, nullptr, false},
  {"Navigator", "mimeTypes", kNavigatorMimeTypes, nullptr, false}
};
const MemberSpec* Find(int32_t slot) {
  for (const auto& spec : kMembers) if (spec.slot == slot) return &spec;
  return nullptr;
}
bool Text(State& state, const char* text, JS::MutableHandleValue result) {
  JS::RootedString string(state.cx, JS_NewStringCopyZ(state.cx, text));
  if (!string) return false;
  result.setString(string);
  return true;
}
// A Plugin/MimeType member's receiver is that interface's instance, and slot 0
// carries the owning State so a wrapper borrowed from another realm is rejected.
bool Instance(JSContext* cx, const JS::CallArgs& args, const MemberSpec& spec,
              int32_t* index) {
  JSObject* object = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  void* owner = object ? JS::GetReservedSlot(object, 0).toPrivate() : nullptr;
  if (!object || JS::GetClass(object) != spec.klass || !owner ||
      owner != js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate()) {
    Error(cx, "PDF plugin member called on incompatible receiver");
    return false;
  }
  *index = JS::GetReservedSlot(object, 1).toInt32();
  return true;
}
State* NavigatorInstance(JSContext* cx, const JS::CallArgs& args) {
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  State* receiver = state ? ReceiverState(args, ClassFor(Kind::Navigator)) : nullptr;
  if (!receiver) {
    ReceiverError(cx, args, "Navigator");
    return nullptr;
  }
  return receiver;
}
// Named results are the donor's legacy named properties: configurable and
// non-writable, absent from enumeration but present in
// Object.getOwnPropertyNames. Indexed results are enumerable.
bool Named(State& state, JS::HandleObject array, const char* name,
           JS::HandleValue value) {
  return JS_DefineProperty(state.cx, array, name, value, JSPROP_READONLY);
}
bool Element(State& state, JS::HandleObject array, uint32_t index,
             JS::HandleValue value) {
  return JS_DefineElement(state.cx, array, index, value,
                          JSPROP_READONLY | JSPROP_ENUMERATE);
}
bool Item(State& state, const MemberSpec& spec, JS::HandleValue value,
          JS::MutableHandleValue result) {
  uint32_t index = 0;
  if (!JS::ToUint32(state.cx, value, &index)) return false;
  const bool plugins = spec.slot == kPluginArrayItem;
  JS::RootedObject array(state.cx,
      plugins ? state.pluginArray.get() : state.mimeTypeArray.get());
  const int32_t count = plugins ? kPluginCount : kMimeCount;
  if (!array) return false;
  result.setNull();
  return index >= uint32_t(count) || JS_GetElement(state.cx, array, index, result);
}
bool NamedItem(State& state, const MemberSpec& spec, JS::HandleValue value,
               JS::MutableHandleValue result) {
  JS::RootedString string(state.cx, JS::ToString(state.cx, value));
  std::string name;
  if (!string || !ToUTF8(state.cx, string, &name)) return false;
  const bool plugins = spec.slot == kPluginArrayNamedItem;
  const char* const* names = plugins ? kPluginNames : kMimeTypeNames;
  const int32_t count = plugins ? kPluginCount : kMimeCount;
  JS::RootedObject array(state.cx,
      plugins ? state.pluginArray.get() : state.mimeTypeArray.get());
  if (!array) return false;
  result.setNull();
  for (int32_t index = 0; index < count; ++index)
    if (name == names[index])
      return JS_GetElement(state.cx, array, uint32_t(index), result);
  return true;
}
bool PluginGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  const MemberSpec* spec = Find(slot);
  if (!spec) return Error(cx, "PDF plugin member is not installed");
  int32_t index = 0;
  State* state = spec->klass ? (Instance(cx, args, *spec, &index) ? static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate()) : nullptr)
                             : NavigatorInstance(cx, args);
  if (!state) return false;
  state->Observe(spec->iface, spec->name, int(ApiAccess::Read));
  switch (slot) {
    case kNavigatorPlugins:
      args.rval().setObject(*state->pluginArray);
      return JS_WrapValue(cx, args.rval());
    case kNavigatorMimeTypes:
      args.rval().setObject(*state->mimeTypeArray);
      return JS_WrapValue(cx, args.rval());
    case kPluginArrayLength: args.rval().setInt32(kPluginCount); return true;
    case kMimeArrayLength: args.rval().setInt32(kMimeCount); return true;
    // Every plugin delegates its length, index and named getters to the one
    // shared MIME type array (nsPluginElement, nsPluginArray.h:116-149).
    case kPluginLength: args.rval().setInt32(kMimeCount); return true;
    case kPluginName: return Text(*state, kPluginNames[index], args.rval());
    case kPluginDescriptionSlot: return Text(*state, kPluginDescription, args.rval());
    case kPluginFilenameSlot: return Text(*state, kPluginFilename, args.rval());
    case kMimeTypeSlot: return Text(*state, kMimeTypeNames[index], args.rval());
    case kMimeTypeDescription: return Text(*state, kMimeDescription, args.rval());
    case kMimeTypeSuffixes: return Text(*state, kMimeSuffixes, args.rval());
    case kMimeTypeEnabledPlugin:
      args.rval().setNull();
      return JS_GetElement(cx, state->pluginArray, 0, args.rval());
    default: return Error(cx, "PDF plugin member is not installed");
  }
}
bool Method(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  const MemberSpec* spec = Find(slot);
  int32_t index = 0;
  if (!spec || !Instance(cx, args, *spec, &index)) return false;
  State* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  state->Observe(spec->iface, spec->name, int(ApiAccess::Call));
  if (slot == kPluginArrayRefresh) { args.rval().setUndefined(); return true; }
  // item/namedItem have one required argument (WebIDL); the donor binding
  // rejects a short argument list before any conversion happens.
  if (args.length() < 1) {
    std::string message = std::string(spec->iface) + "." + spec->name +
        ": At least 1 argument required, but only " +
        std::to_string(args.length()) + " passed";
    return Error(cx, message.c_str());
  }
  if (slot == kPluginArrayItem || slot == kPluginItem || slot == kMimeArrayItem)
    return Item(*state, *spec, args.get(0), args.rval());
  return NamedItem(*state, *spec, args.get(0), args.rval());
}
bool Constructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return Error(cx, "Illegal constructor.");
}
// One interface: prototype object + throwing constructor + [Symbol.toStringTag],
// installed as a global property and registered with the interface trace, the
// same way every other owner installs a constructor.
bool Interface(State& state, const JSClass* klass, JS::MutableHandleObject result) {
  JSContext* cx = state.cx;
  JS::RootedObject prototype(cx, JS_NewPlainObject(cx));
  JS::RootedObject constructor(cx,
      state.Function(Constructor, klass->name, 0, 0, JSFUN_CONSTRUCTOR));
  if (!prototype || !constructor) return false;
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, klass->name));
  JS::RootedId tagId(cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  if (!tag || !JS_DefineProperty(cx, constructor, "prototype", prototype,
                                 JSPROP_READONLY | JSPROP_PERMANENT)) return false;
  // Firefox's own-key order: operations and attributes, constructor,
  // @@iterator, @@toStringTag.
  for (const auto& spec : kMembers) {
    if (spec.klass != klass) continue;
    if (spec.method) {
      const unsigned arity = spec.slot == kPluginArrayRefresh ? 0 : 1;
      JS::RootedObject method(cx, state.Function(Method, spec.name, arity, spec.slot));
      if (!method || !JS_DefineProperty(cx, prototype, spec.name, method,
                                        JSPROP_ENUMERATE) ||
          !state.helpers->Member(method, spec.iface, spec.name, "call")) return false;
    } else if (!state.Accessor(prototype, spec.name, spec.slot, JSPROP_ENUMERATE,
                               PluginGetter) ||
               !state.helpers->Accessor(spec.iface, prototype, spec.name)) return false;
  }
  if (!JS_DefineProperty(cx, prototype, "constructor", constructor, 0)) return false;
  // WebIDL: the three interfaces with an indexed getter and an integer length
  // get %Array.prototype.values% as their @@iterator (writable, configurable).
  if (klass != &kMimeTypeClass) {
    JS::RootedObject arrayPrototype(cx, JS::GetRealmArrayPrototype(cx));
    JS::RootedValue values(cx);
    JS::RootedId iteratorId(cx, JS::PropertyKey::Symbol(
        JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
    if (!arrayPrototype || !JS_GetProperty(cx, arrayPrototype, "values", &values) ||
        !JS_DefinePropertyById(cx, prototype, iteratorId, values, 0)) return false;
  }
  if (!JS_DefinePropertyById(cx, prototype, tagId, tag, JSPROP_READONLY)) return false;
  if (!JS_DefineProperty(cx, state.page, klass->name, constructor, 0) ||
      !state.helpers->Interface(klass->name, prototype) ||
      !state.helpers->Object(klass->name, prototype) ||
      !state.helpers->Constructor(klass->name, constructor)) return false;
  result.set(prototype);
  return true;
}
// Every plugin delegates its indexed and named getters to the one shared MIME
// type array (nsPluginElement, nsPluginArray.h:116-149), so the same two
// objects are own properties of the array and of each plugin. Indexed results
// are enumerable, named results are the donor's legacy named properties.
bool MimeTypes(State& state, JS::HandleObject target, JS::HandleObject array) {
  for (int32_t index = 0; index < kMimeCount; ++index) {
    JS::RootedValue value(state.cx);
    if (!JS_GetElement(state.cx, array, uint32_t(index), &value)) return false;
    if (!Element(state, target, uint32_t(index), value) ||
        !Named(state, target, kMimeTypeNames[index], value)) return false;
  }
  return true;
}
}  // namespace

bool InstallPluginSurface(State& state) {
  JSContext* cx = state.cx;
  JS::RootedObject mimePrototype(cx), mimeArrayPrototype(cx);
  JS::RootedObject pluginPrototype(cx), pluginArrayPrototype(cx);
  if (!Interface(state, &kMimeTypeClass, &mimePrototype) ||
      !Interface(state, &kMimeTypeArrayClass, &mimeArrayPrototype) ||
      !Interface(state, &kPluginClass, &pluginPrototype) ||
      !Interface(state, &kPluginArrayClass, &pluginArrayPrototype)) return false;
  // The MIME type array lands first: every plugin's getters delegate to it.
  JS::RootedObject mimeArray(cx, JS_NewObjectWithGivenProto(cx, &kMimeTypeArrayClass,
                                                            mimeArrayPrototype));
  if (!mimeArray) return false;
  JS::SetReservedSlot(mimeArray, 0, JS::PrivateValue(&state));
  JS::SetReservedSlot(mimeArray, 1, JS::Int32Value(0));
  for (int32_t index = 0; index < kMimeCount; ++index) {
    JS::RootedObject element(cx,
        JS_NewObjectWithGivenProto(cx, &kMimeTypeClass, mimePrototype));
    if (!element) return false;
    JS::SetReservedSlot(element, 0, JS::PrivateValue(&state));
    JS::SetReservedSlot(element, 1, JS::Int32Value(index));
    JS::RootedValue value(cx, JS::ObjectValue(*element));
    if (!Element(state, mimeArray, uint32_t(index), value) ||
        !Named(state, mimeArray, kMimeTypeNames[index], value)) return false;
  }
  state.mimeTypeArray = mimeArray;
  JS::RootedObject pluginArray(cx, JS_NewObjectWithGivenProto(cx, &kPluginArrayClass,
                                                              pluginArrayPrototype));
  if (!pluginArray) return false;
  JS::SetReservedSlot(pluginArray, 0, JS::PrivateValue(&state));
  JS::SetReservedSlot(pluginArray, 1, JS::Int32Value(0));
  for (int32_t index = 0; index < kPluginCount; ++index) {
    JS::RootedObject element(cx,
        JS_NewObjectWithGivenProto(cx, &kPluginClass, pluginPrototype));
    if (!element) return false;
    JS::SetReservedSlot(element, 0, JS::PrivateValue(&state));
    JS::SetReservedSlot(element, 1, JS::Int32Value(index));
    if (!MimeTypes(state, element, mimeArray)) return false;
    JS::RootedValue value(cx, JS::ObjectValue(*element));
    if (!Element(state, pluginArray, uint32_t(index), value) ||
        !Named(state, pluginArray, kPluginNames[index], value)) return false;
  }
  state.pluginArray = pluginArray;
  // The two Navigator accessors are appended to the prototype the Navigator
  // instance is built from, exactly like the WebGPU owner's single accessor.
  for (const auto& spec : kMembers) {
    if (spec.klass) continue;
    if (!state.Accessor(state.navigatorPrototype, spec.name, spec.slot,
                        JSPROP_ENUMERATE, PluginGetter) ||
        !state.helpers->Accessor("Navigator", state.navigatorPrototype, spec.name))
      return false;
  }
  return true;
}
}  // namespace ruyijs::browser_environment
