#include "web_media/media_key_internal.h"
#include "web_media/media_runtime.h"
#include "runtime/page_helpers.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_media {
const JSClass kMediaKeyErrorClass = {"MediaKeyError", JSCLASS_HAS_RESERVED_SLOTS(ErrorSlotCount)};

namespace {
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) { return WebKit::Reject(cx, argc, vp); }
bool SystemCode(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); JS::RootedObject self(cx);
  if (!MediaReceiver(cx, args.thisv(), &kMediaKeyErrorClass, &self)) return false;
  args.rval().setInt32(JS::GetReservedSlot(self, ErrorSystemCode).toInt32()); return true;
}
}

bool InstallMediaKeyError(State& state) {
  JS::RootedValue eventCtor(state.cx);
  if (!JS_GetProperty(state.cx, state.kit.page, "Event", &eventCtor) || !eventCtor.isObject()) return false;
  JS::RootedObject event(state.cx, &eventCtor.toObject());
  JS::RootedValue parentValue(state.cx);
  if (!JS_GetProperty(state.cx, event, "prototype", &parentValue) || !parentValue.isObject()) return false;
  JS::RootedObject parent(state.cx, &parentValue.toObject());
  return state.kit.Define("MediaKeyError", &kMediaKeyErrorClass, parent, Illegal, &state.keyError) &&
      state.kit.Accessor(state.keyError, "MediaKeyError", "systemCode", SystemCode, nullptr, ErrorSystemCode) &&
      MediaConstructorLast(state, state.keyError);
}
}
