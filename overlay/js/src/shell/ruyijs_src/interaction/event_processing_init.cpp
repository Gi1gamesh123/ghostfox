#include "interaction/internal.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"

namespace ruyijs::interaction {
namespace {
bool ClearConversion(JSContext* cx) {
  if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
  JS_ClearPendingException(cx);
  return true;
}
bool Reference(JSContext* cx, JS::HandleObject dict, JS::HandleObject object,
               const char* name, uint32_t slot) {
  JS::RootedValue value(cx);
  if (!PublicEventRead(cx, dict, name, &value)) return false;
  JS::SetReservedSlot(object, slot, value.isUndefined() ? JS::NullValue() : value.get());
  return true;
}
}
bool InitProcessingFields(JSContext* cx, Iface iface, JS::HandleObject dict, JS::HandleObject object) {
  JS::RootedValue value(cx);
  if (iface == Iface::FontLoad || iface == Iface::GpuUncaptured)
    return PublicEventRead(cx, dict, iface == Iface::FontLoad ? "fontfaces" : "error", &value);
  if (iface == Iface::TaskPriority) {
    if (!dict)return Error(cx,"TypeError","previousPriority is required");
    if (!JS_GetProperty(cx, dict, "previousPriority", &value)) return false;
    if(value.isUndefined())return Error(cx,"TypeError","previousPriority is required");
    JS::RootedString text(cx,JS::ToString(cx,value));if(!text)return false;
    bool valid=false;for(const char* candidate:{"user-blocking","user-visible","background"}){
      bool equal=false;if(!JS_StringEqualsAscii(cx,text,candidate,&equal))return false;valid|=equal;}
    if(!valid)return Error(cx,"TypeError","Invalid TaskPriority");
    JS::SetReservedSlot(object, kEventPublicFirst, JS::StringValue(text));
    return true;
  }
  if (!PublicEventRead(cx, dict, "playbackTime", &value)) return false;
  double time = 0;
  if (!value.isUndefined() && !JS::ToNumber(cx, value, &time)) {
    if (!ClearConversion(cx)) return false;
    time = 0;
  }
  JS::SetReservedSlot(object, kEventPublicFirst, JS::NumberValue(time));
  return Reference(cx, dict, object, "inputBuffer", kEventPublicSecond) &&
         Reference(cx, dict, object, "outputBuffer", kEventPublicThird);
}
}  // namespace ruyijs::interaction
