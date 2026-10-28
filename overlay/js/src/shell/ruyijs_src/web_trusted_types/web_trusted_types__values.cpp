// TrustedHTML / TrustedScript / TrustedScriptURL: immutable string holders with
// a stringifier and toJSON, plus the engine's HostGetCodeForEval hook.
#include "web_trusted_types/internal.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include <string>

namespace ruyijs::web_trusted_types {
const JSClass kValueClasses[3] = {
    {"TrustedHTML", JSCLASS_HAS_RESERVED_SLOTS(1)},
    {"TrustedScript", JSCLASS_HAS_RESERVED_SLOTS(1)},
    {"TrustedScriptURL", JSCLASS_HAS_RESERVED_SLOTS(1)}};

bool Fail(JSContext* cx, const char* message) { return WebKit::Fail(cx, message); }
bool Arity(JSContext* cx, const JS::CallArgs& args, unsigned required, const char* method) {
  if (args.length() >= required) return true;
  std::string text = std::string(method) + ": At least " + std::to_string(required) +
      " argument required, but only " + std::to_string(args.length()) + " passed";
  return Fail(cx, text.c_str());
}
bool Receiver(JSContext* cx, JS::HandleValue self, const JSClass* clasp, const char* member,
              const char* iface, JS::MutableHandleObject out) {
  JSObject* object = self.isObject() ? js::CheckedUnwrapStatic(&self.toObject()) : nullptr;
  if (!object || JS::GetClass(object) != clasp) {
    std::string text = std::string("'") + member +
        "' called on an object that does not implement interface " + iface + ".";
    return Fail(cx, text.c_str());
  }
  out.set(object);
  return true;
}
bool IsValue(JSObject* object, Kind kind) {
  JSObject* unwrapped = object ? js::CheckedUnwrapStatic(object) : nullptr;
  return unwrapped && JS::GetClass(unwrapped) == &kValueClasses[int(kind)];
}
JSObject* NewValue(State& state, Kind kind, JS::HandleString data) {
  JS::RootedObject object(state.cx, state.kit.New(&kValueClasses[int(kind)],
                                                  state.Face(kind).prototype));
  if (object) JS::SetReservedSlot(object, 0, JS::StringValue(data));
  return object;
}
bool FinishPrototype(State& state, WebFace& face) {
  JS::RootedValue constructor(state.cx, JS::ObjectValue(*face.constructor));
  return JS_DeleteProperty(state.cx, face.prototype, "constructor") &&
         JS_DefineProperty(state.cx, face.prototype, "constructor", constructor, 0);
}
bool CodeForEval(JSContext* cx, JS::HandleObject code, JS::MutableHandle<JSString*> out) {
  if (!IsValue(code, Kind::Script)) {
    out.set(nullptr);
    return true;
  }
  JS::RootedValue value(cx, JS::GetReservedSlot(js::CheckedUnwrapStatic(code), 0));
  if (!JS_WrapValue(cx, &value)) return false;
  out.set(value.toString());
  return true;
}
namespace {
// toString (the stringifier) is slot*2, toJSON slot*2+1, so one native serves both.
bool Stringify(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  const Kind kind = Kind(slot / 2);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kValueClasses[int(kind)], slot % 2 ? "toJSON" : "toString",
                kValueClasses[int(kind)].name, &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, 0));
  return JS_WrapValue(cx, args.rval());
}
}
bool InstallValues(State& state) {
  for (int index = 0; index < 3; ++index) {
    const JSClass& clasp = kValueClasses[index];
    WebFace& face = state.Face(Kind(index));
    if (!state.kit.Define(clasp.name, &clasp, nullptr, WebKit::Reject, &face) ||
        !state.kit.Method(face, clasp.name, "toJSON", Stringify, 0, index * 2 + 1) ||
        !state.kit.Method(face, clasp.name, "toString", Stringify, 0, index * 2) ||
        !FinishPrototype(state, face)) return false;
  }
  return true;
}
}
