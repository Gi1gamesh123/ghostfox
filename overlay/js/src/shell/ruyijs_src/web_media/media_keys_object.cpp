#include "web_media/media_key_internal.h"
#include "web_media/media_runtime.h"
#include "runtime/page_helpers.h"
#include "js/Object.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include "jsapi.h"

namespace ruyijs::web_media {
const JSClass kMediaKeysClass = {"MediaKeys", JSCLASS_HAS_RESERVED_SLOTS(KeysSlotCount)};

namespace {
bool Receiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject out) {
  return MediaReceiver(cx, args.thisv(), &kMediaKeysClass, out);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false;
  return MediaGetSlot(cx, self, KeysKeySystem, args.rval());
}
bool Method(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false;
  switch (WebKit::Slot(args)) {
    case 0: {
      auto& state = *static_cast<State*>(WebKit::Owner(args));
      JS::RootedObject session(cx); if (!NewMediaKeySession(state, &session)) return false;
      JS::RootedValue value(cx, JS::ObjectValue(*session)); return PromiseValue(cx, value, args.rval());
    }
    case 1: return PromiseUndefined(cx, args.rval());
    default: {
      JS::RootedString status(cx, JS_NewStringCopyZ(cx, "usable"));
      JS::RootedValue value(cx, JS::StringValue(status));
      return status && PromiseValue(cx, value, args.rval());
    }
  }
}
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) { return WebKit::Reject(cx, argc, vp); }
}

bool IsMediaKeysValue(JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  return object && JS::GetClass(object) == &kMediaKeysClass;
}
bool NewMediaKeys(State& state, JS::HandleString keySystem, JS::MutableHandleObject out) {
  JS::RootedObject object(state.cx, state.kit.New(&kMediaKeysClass, state.keys.prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, KeysKeySystem, JS::StringValue(keySystem)); out.set(object); return true;
}
bool InstallMediaKeysObject(State& state) {
  JS::RootedObject parent(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!state.kit.Define("MediaKeys", &kMediaKeysClass, parent, Illegal, &state.keys) ||
      !state.kit.Method(state.keys, "MediaKeys", "createSession", Method, 0, 0) ||
      !state.kit.Method(state.keys, "MediaKeys", "setServerCertificate", Method, 1, 1) ||
      !state.kit.Method(state.keys, "MediaKeys", "getStatusForPolicy", Method, 0, 2) ||
      !state.kit.Accessor(state.keys, "MediaKeys", "keySystem", Get, nullptr, KeysKeySystem)) return false;
  return MediaConstructorLast(state, state.keys);
}
}
