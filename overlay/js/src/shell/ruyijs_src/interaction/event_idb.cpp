// Version payload event only; database versions and requests belong to IndexedDB.
#include "interaction/internal.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include <cmath>

namespace ruyijs::interaction {
namespace {
bool Get(JSContext* cx, const JS::CallArgs& args, bool next) {
  Event* event = Receiver(cx, args, Iface::IdbVersionChange, true);
  if (!event) return false;
  ReportEventAccess(cx, event, "IDBVersionChangeEvent", next ? "newVersion" : "oldVersion", int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(event->object, next ? kEventPublicSecond : kEventPublicFirst));
  return JS_WrapValue(cx, args.rval());
}
bool OldVersion(JSContext* cx, unsigned n, JS::Value* vp) {
  return Get(cx, JS::CallArgsFromVp(n, vp), false);
}
bool NewVersion(JSContext* cx, unsigned n, JS::Value* vp) {
  return Get(cx, JS::CallArgsFromVp(n, vp), true);
}
bool Version(JSContext* cx, JS::HandleObject dict, JS::HandleObject object,
             const char* name, uint32_t slot, bool nullable) {
  JS::RootedValue value(cx);
  if (!PublicEventRead(cx, dict, name, &value)) return false;
  if (nullable && value.isNullOrUndefined()) {
    JS::SetReservedSlot(object, slot, JS::NullValue());
    return true;
  }
  double number = 0;
  if (!JS::ToNumber(cx, value, &number)) {
    if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
    JS_ClearPendingException(cx);
    number = 0;
  }
  // Public ruyijs uses a signed-64 conversion before the unsigned projection.
  // Check its sentinel boundary explicitly instead of casting out-of-range doubles.
  double result = 0;
  if (!std::isnan(number)) {
    if (number >= 0x1p63 || number < -0x1p63) result = 0x1p63;
    else result = static_cast<double>(static_cast<uint64_t>(static_cast<int64_t>(number)));
  }
  JS::SetReservedSlot(object, slot, JS::NumberValue(result));
  return true;
}
}
extern const JSPropertySpec kIdbVersionChangeProperties[] = {
  JS_PSG("oldVersion", OldVersion, JSPROP_ENUMERATE),
  JS_PSG("newVersion", NewVersion, JSPROP_ENUMERATE), JS_PS_END};

bool InitIdbVersionChange(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  return Version(cx, dict, object, "oldVersion", kEventPublicFirst, false) &&
         Version(cx, dict, object, "newVersion", kEventPublicSecond, true);
}
}  // namespace ruyijs::interaction
