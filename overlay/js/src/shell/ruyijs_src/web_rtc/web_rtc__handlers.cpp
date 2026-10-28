// The shared on* EventHandler accessors of RTCPeerConnection, RTCDataChannel
// and the three transports, and the owner's WebIDL brand check. A handler is
// registered through the object's own add/removeEventListener, so a handler
// and addEventListener(listener) are the same registration.
#include "web_rtc/state.h"

#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/ValueArray.h"

namespace ruyijs::web_rtc {

static const char* HandlerName(const JSClass* clasp, int32_t slot) {
  if (clasp == &kPeerClass && slot >= kPeerHandlerBase && slot < kPeerSlotCount)
    return kPeerHandlers[slot - kPeerHandlerBase];
  if (clasp == &kChannelClass && slot >= kChannelHandlerBase && slot < kChannelSlotCount)
    return kChannelHandlers[slot - kChannelHandlerBase];
  if (clasp == &kDtlsClass && slot >= kDtlsHandlerBase && slot < kDtlsSlotCount)
    return kDtlsHandlers[slot - kDtlsHandlerBase];
  if (clasp == &kIceClass && slot >= kIceHandlerBase && slot < kIceSlotCount)
    return kIceHandlers[slot - kIceHandlerBase];
  if (clasp == &kSctpClass && slot >= kSctpHandlerBase && slot < kSctpSlotCount)
    return kSctpHandlers[slot - kSctpHandlerBase];
  return nullptr;
}

static bool Delegate(JSContext* cx, JS::HandleObject self, const char* method,
                     const char* type, JS::HandleValue callback) {
  JS::RootedValue function(cx);
  if (!JS_GetProperty(cx, self, method, &function)) return false;
  if (!function.isObject() || !JS::IsCallable(&function.toObject())) return true;
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, type));
  if (!name) return false;
  JS::RootedValueArray<2> arguments(cx);
  arguments[0].setString(name);
  arguments[1].set(callback);
  JS::RootedValue ignored(cx);
  return JS::Call(cx, self, function, arguments, &ignored);
}

// The interfaces whose on* accessors share HandlerGet/HandlerSet, by tag.
static const JSClass* const kHandlerClasses[] = {&kPeerClass, &kChannelClass, &kDtlsClass,
                                                  &kIceClass, &kSctpClass};

int32_t HandlerSlot(const JSClass* clasp, int32_t slot) {
  for (int32_t i = 0; i < 5; ++i)
    if (kHandlerClasses[i] == clasp) return (i << 8) | slot;
  return slot;
}

bool Brand(JSContext* cx, const JS::CallArgs& args, const JSClass* clasp,
           JS::MutableHandleObject self) {
  if (args.thisv().isObject() && JS::GetClass(&args.thisv().toObject()) == clasp) {
    self.set(&args.thisv().toObject());
    return true;
  }
  return WebKit::FailBrand(cx, args, clasp->name);
}

// Reads the tagged slot and brand-checks against the accessor's own interface.
static bool HandlerReceiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject self,
                            int32_t* slot, const char** type) {
  const int32_t tagged = WebKit::Slot(args);
  *slot = tagged & 0xff;
  if (!Brand(cx, args, kHandlerClasses[(tagged >> 8) % 5], self)) return false;
  *type = HandlerName(JS::GetClass(self), *slot);
  return *type || WebKit::Fail(cx, "WebRTC event handler slot is invalid");
}

bool HandlerGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  int32_t slot = 0;
  const char* type = nullptr;
  if (!HandlerReceiver(cx, args, &self, &slot, &type)) return false;
  args.rval().set(JS::GetReservedSlot(self, slot));
  return true;
}

bool HandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  int32_t slot = 0;
  const char* type = nullptr;
  if (!HandlerReceiver(cx, args, &self, &slot, &type)) return false;
  JS::RootedValue old(cx, JS::GetReservedSlot(self, slot));
  JS::RootedValue value(cx, args.get(0));
  const bool callable = value.isObject() && JS::IsCallable(&value.toObject());
  if (old.isObject() && !Delegate(cx, self, "removeEventListener", type, old)) return false;
  if (callable && !Delegate(cx, self, "addEventListener", type, value)) return false;
  if (!callable) value.setNull();
  JS::SetReservedSlot(self, slot, value);
  args.rval().setUndefined();
  return true;
}

}  // namespace ruyijs::web_rtc
