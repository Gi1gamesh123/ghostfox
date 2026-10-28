#include "web_media/media_key_internal.h"
#include "web_media/media_runtime.h"
#include "runtime/page_helpers.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_media {
const JSClass kMediaKeySystemAccessClass = {"MediaKeySystemAccess", JSCLASS_HAS_RESERVED_SLOTS(AccessSlotCount)};

namespace {
bool Receiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject out) {
  return MediaReceiver(cx, args.thisv(), &kMediaKeySystemAccessClass, out);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false;
  return MediaGetSlot(cx, self, WebKit::Slot(args), args.rval());
}
bool Method(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false;
  if (WebKit::Slot(args) == 0) return MediaGetSlot(cx, self, AccessConfiguration, args.rval());
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedValue keyValue(cx, JS::GetReservedSlot(self, AccessKeySystem));
  JS::RootedString key(cx, keyValue.isString() ? keyValue.toString() : nullptr);
  JS::RootedObject keys(cx);
  if (!key || !NewMediaKeys(state, key, &keys)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*keys));
  return PromiseValue(cx, value, args.rval());
}
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) { return WebKit::Reject(cx, argc, vp); }
}

bool InstallMediaKeySystemAccess(State& state) {
  JS::RootedObject parent(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!state.kit.Define("MediaKeySystemAccess", &kMediaKeySystemAccessClass, parent,
                        Illegal, &state.keySystemAccess) ||
      !state.kit.Method(state.keySystemAccess, "MediaKeySystemAccess", "getConfiguration", Method, 0, 0) ||
      !state.kit.Method(state.keySystemAccess, "MediaKeySystemAccess", "createMediaKeys", Method, 0, 1) ||
      !state.kit.Accessor(state.keySystemAccess, "MediaKeySystemAccess", "keySystem", Get, nullptr, AccessKeySystem)) return false;
  return MediaConstructorLast(state, state.keySystemAccess);
}
}
