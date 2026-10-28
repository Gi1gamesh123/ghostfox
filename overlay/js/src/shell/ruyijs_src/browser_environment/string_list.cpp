// The DOMStringList used by Location.ancestorOrigins (T10). Firefox exposes it
// as a real interface: a read-only indexed list with length/item/contains whose
// prototype is the realm's own DOMStringList.prototype. The values live in one
// frozen JS array held in a reserved slot, so the object graph stays inside this
// realm and the class needs no business state of its own.
#include "browser_environment/string_list.h"
#include "browser_environment/state.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/runtime.h"
#include "runtime/page_helpers.h"
#include "js/Conversions.h"
#include "jsfriendapi.h"
#include "js/Wrapper.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
#include "jsapi.h"
#include <string>
#include <vector>

namespace ruyijs::browser_environment {
namespace {
// Slot 0 is the owning State, slot 1 the frozen array of strings. Reserved
// slots are traced by the GC, so the array stays alive with the list.
const JSClass kStringListClass = {"DOMStringList", JSCLASS_HAS_RESERVED_SLOTS(2)};

bool Constructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return Error(cx, "Illegal constructor.");
}

// Every member checks the class and reads the list's own State, so the
// prototype itself or another object is a TypeError rather than a silent read.
bool Receiver(JSContext* cx, const JS::CallArgs& args, State** state,
              JS::MutableHandleObject values) {
  State* owner = ReceiverState(args, &kStringListClass);
  if (!owner) {
    Error(cx, "DOMStringList member called on incompatible receiver");
    return false;
  }
  JS::RootedObject self(cx, js::CheckedUnwrapStatic(&args.thisv().toObject()));
  *state = owner;
  values.set(&JS::GetReservedSlot(self, 1).toObject());
  return true;
}
}  // namespace

JSObject* NewStringList(State& state, const std::vector<std::string>& values) {
  JSContext* cx = state.cx;
  JS::RootedObject array(cx, JS::NewArrayObject(cx, values.size()));
  if (!array) return nullptr;
  for (size_t index = 0; index < values.size(); ++index) {
    const std::string& source = values[index];
    JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx,
        JS::UTF8Chars(source.data(), source.size())));
    if (!text || !JS_DefineElement(cx, array, index, text, JSPROP_ENUMERATE)) return nullptr;
  }
  return NewStringListArray(state,array);
}
JSObject* NewStringListArray(State& state,JS::HandleObject array) {
  JSContext* cx=state.cx;
  if (!JS_FreezeObject(cx, array)) return nullptr;
  JS::RootedObject list(cx, JS_NewObjectWithGivenProto(cx, &kStringListClass,
                                                       state.stringListPrototype));
  if (!list) return nullptr;
  JS::SetReservedSlot(list, 0, JS::PrivateValue(&state));
  JS::SetReservedSlot(list, 1, JS::ObjectValue(*array));
  // DOMStringList's indexed getter over the immutable snapshot: read-only,
  // enumerable own index properties (legacy platform object approximation).
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, array, &length)) return nullptr;
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedValue item(cx);
    if (!JS_GetElement(cx, array, index, &item) ||
        !JS_DefineElement(cx, list, index, item, JSPROP_ENUMERATE | JSPROP_READONLY)) return nullptr;
  }
  return list;
}

bool StringListGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  State* state = nullptr;
  JS::RootedObject values(cx);
  if (!Receiver(cx, args, &state, &values)) return false;
  if (slot != kStringListLength) return Error(cx, "DOMStringList member is not installed");
  state->Observe("DOMStringList", "length", int(ApiAccess::Read));
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, values, &length)) return false;
  args.rval().setInt32(int32_t(length));
  return true;
}

bool StringListMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  State* state = nullptr;
  JS::RootedObject values(cx);
  if (!Receiver(cx, args, &state, &values)) return false;
  if (slot == kStringListItem || slot == kStringListContains)
    state->Observe("DOMStringList", slot == kStringListItem ? "item" : "contains",
                   int(ApiAccess::Call));
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, values, &length)) return false;
  if (slot == kStringListItem) {
    int32_t index = 0;
    if (!JS::ToInt32(cx, args.get(0), &index)) return false;
    if (index < 0 || uint32_t(index) >= length) { args.rval().setNull(); return true; }
    JS::RootedValue element(cx);
    if (!JS_GetElement(cx, values, uint32_t(index), &element)) return false;
    args.rval().set(element);
    return true;
  }
  if (slot == kStringListContains) {
    JS::RootedString target(cx, JS::ToString(cx, args.get(0)));
    std::string needle;
    if (!target || !ToUTF8(cx, target, &needle)) return false;
    for (uint32_t index = 0; index < length; ++index) {
      JS::RootedValue element(cx);
      if (!JS_GetElement(cx, values, index, &element)) return false;
      if (!element.isString()) continue;
      JS::RootedString text(cx, element.toString());
      std::string value;
      if (!ToUTF8(cx, text, &value)) return false;
      if (value == needle) { args.rval().setBoolean(true); return true; }
    }
    args.rval().setBoolean(false);
    return true;
  }
  return Error(cx, "DOMStringList member is not installed");
}

// Location.ancestorOrigins: the ancestor browsing contexts' origins, nearest
// first. A child realm reports the origin it inherits from its creator, which
// is the one origin this runtime's about:blank frames all share; a top-level
// browsing context has no ancestors and reports an empty list.
bool AncestorOriginsGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  State* receiver = state ? ReceiverState(args, ClassFor(Kind::Location)) : nullptr;
  if (!receiver) {
    Error(cx, "Location member called on incompatible receiver");
    return false;
  }
  state = receiver;
  state->Observe("Location", "ancestorOrigins", int(ApiAccess::Read));
  std::vector<std::string> origins;
  if (state->shared) {
    JS::RootedValue origin(cx);
    if (!state->Read(Kind::Location, 8, ValueType::String, &origin)) return false;
    JS::RootedString text(cx, origin.toString());
    std::string value;
    if (!ToUTF8(cx, text, &value)) return false;
    origins.push_back(std::move(value));
  }
  JSObject* list = NewStringList(*state, origins);
  if (!list) return false;
  args.rval().setObject(*list);
  return true;
}

bool InstallAncestorOrigins(State& state, JS::HandleObject location) {
  return state.Accessor(location, "ancestorOrigins", 0,
                        JSPROP_ENUMERATE | JSPROP_PERMANENT, AncestorOriginsGetter) &&
         state.helpers->Accessor("Location", location, "ancestorOrigins");
}

bool InstallStringList(State& state) {
  JSContext* cx = state.cx;
  JS::RootedObject prototype(cx, JS_NewPlainObject(cx));
  JS::RootedObject constructor(cx, state.Function(Constructor, "DOMStringList", 0, 0,
                                                  JSFUN_CONSTRUCTOR));
  JS::RootedObject objectPrototype(cx, JS::GetRealmObjectPrototype(cx));
  if (!prototype || !constructor || !objectPrototype ||
      !JS_SetPrototype(cx, prototype, objectPrototype)) return false;
  // Firefox's own-key order: operations, attributes, constructor, @@iterator,
  // @@toStringTag.
  struct MethodSpec { const char* name; int32_t slot; unsigned length; };
  static const MethodSpec kMethods[] = {
    {"item", kStringListItem, 1}, {"contains", kStringListContains, 1},
  };
  for (const auto& spec : kMethods) {
    JS::RootedObject method(cx, state.Function(StringListMethod, spec.name, spec.length,
                                               spec.slot));
    if (!method || !JS_DefineProperty(cx, prototype, spec.name, method, 0) ||
        !state.helpers->Member(method, "DOMStringList", spec.name, "call")) return false;
  }
  if (!state.Accessor(prototype, "length", kStringListLength, JSPROP_ENUMERATE,
                      StringListGetter) ||
      !state.helpers->Accessor("DOMStringList", prototype, "length")) return false;
  if (!JS_DefineProperty(cx, constructor, "prototype", prototype,
                         JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(cx, prototype, "constructor", constructor, 0)) return false;
  // WebIDL: an interface with an indexed getter and an integer length gets
  // %Array.prototype.values% as its @@iterator (writable, configurable).
  JS::RootedObject arrayPrototype(cx, JS::GetRealmArrayPrototype(cx));
  JS::RootedValue values(cx);
  JS::RootedId iteratorId(cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
  if (!arrayPrototype || !JS_GetProperty(cx, arrayPrototype, "values", &values) ||
      !JS_DefinePropertyById(cx, prototype, iteratorId, values, 0)) return false;
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, "DOMStringList"));
  JS::RootedId tagId(cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  if (!tag || !JS_DefinePropertyById(cx, prototype, tagId, tag, JSPROP_READONLY)) return false;
  state.stringListPrototype = prototype;
  return JS_DefineProperty(cx, state.page, "DOMStringList", constructor, 0) &&
         state.helpers->Interface("DOMStringList", prototype) &&
         state.helpers->Object("DOMStringList", prototype) &&
         state.helpers->Constructor("DOMStringList", constructor);
}
}  // namespace ruyijs::browser_environment
