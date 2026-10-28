// VTTRegion (WebVTT regions), beside VTTCue in the text-track surface of the
// web_media owner. A region is a plain value object: its eight attributes live
// in reserved slots; Firefox 158 defaults, ranges and error texts were measured
// (artifacts/mouser-datadome/vttregion_probe.js).
#include "web_media/text_track.h"
#include "web_media/media_runtime.h"
#include "web_media/state.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cmath>
#include <string>

namespace ruyijs::web_media {
const JSClass kVTTRegionClass = {"VTTRegion", JSCLASS_HAS_RESERVED_SLOTS(RegionSlotCount)};
namespace {
constexpr const char* kFields[RegionSlotCount] = {"id", "width", "lines", "regionAnchorX",
    "regionAnchorY", "viewportAnchorX", "viewportAnchorY", "scroll"};
bool Receiver(JSContext* cx, const JS::CallArgs& args, const char* verb, JS::MutableHandleObject out) {
  out.set(args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (out && JS::GetClass(out) == &kVTTRegionClass) return true;
  const std::string message = std::string("'") + verb + " " + kFields[WebKit::Slot(args)] +
      "' called on an object that does not implement interface VTTRegion.";
  return WebKit::Fail(cx, message.c_str());
}
bool Constructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.isConstructing()) return WebKit::Fail(cx, "VTTRegion constructor: 'new' is required");
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject proto(cx);
  if (!state.kit.ConstructProto(args, state.vttRegion.prototype, &proto)) return false;
  JS::RootedObject region(cx, JS_NewObjectWithGivenProto(cx, &kVTTRegionClass, proto));
  if (!region) return false;
  JS::SetReservedSlot(region, RegionId, JS::StringValue(JS_GetEmptyString(cx)));
  JS::SetReservedSlot(region, RegionWidth, JS::DoubleValue(100));
  JS::SetReservedSlot(region, RegionLines, JS::Int32Value(3));
  JS::SetReservedSlot(region, RegionAnchorX, JS::DoubleValue(0));
  JS::SetReservedSlot(region, RegionAnchorY, JS::DoubleValue(100));
  JS::SetReservedSlot(region, RegionViewportX, JS::DoubleValue(0));
  JS::SetReservedSlot(region, RegionViewportY, JS::DoubleValue(100));
  JS::SetReservedSlot(region, RegionScroll, JS::StringValue(JS_GetEmptyString(cx)));
  args.rval().setObject(*region);
  return true;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, "get", &self)) return false;
  JS::RootedValue value(cx, JS::GetReservedSlot(self, WebKit::Slot(args)));
  if (!JS_WrapValue(cx, &value)) return false;
  args.rval().set(value);
  return true;
}
bool IndexSize(State& state) {
  return MediaException(state, "IndexSizeError", "Index or size is negative or greater than the allowed amount");
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, "set", &self)) return false;
  // rval() shares the callee slot: read the owner and slot first.
  const int slot = WebKit::Slot(args);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  args.rval().setUndefined();
  if (slot == RegionId || slot == RegionScroll) {
    JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
    if (!text) return false;
    if (slot == RegionScroll) {  // ScrollSetting enum: "" or "up"; other values are ignored.
      bool up = false, empty = JS_GetStringLength(text) == 0;
      if (!JS_StringEqualsAscii(cx, text, "up", &up)) return false;
      if (!up && !empty) return true;
    }
    JS::SetReservedSlot(self, slot, JS::StringValue(text));
    return true;
  }
  if (slot == RegionLines) {  // long: ToInt32, negative is out of range.
    int32_t lines = 0;
    if (!JS::ToInt32(cx, args.get(0), &lines)) return false;
    if (lines < 0) return IndexSize(state);
    JS::SetReservedSlot(self, slot, JS::Int32Value(lines));
    return true;
  }
  double number = 0;  // double percentages in [0, 100]
  if (!JS::ToNumber(cx, args.get(0), &number)) return false;
  if (!std::isfinite(number)) {
    const std::string message = std::string("VTTRegion.") + kFields[slot] +
        " setter: Value being assigned is not a finite floating-point value.";
    return WebKit::Fail(cx, message.c_str());
  }
  if (number < 0 || number > 100) return IndexSize(state);
  JS::SetReservedSlot(self, slot, JS::DoubleValue(number));
  return true;
}
}

bool IsVTTRegion(JSObject* object) {
  JSObject* unwrapped = object ? js::CheckedUnwrapStatic(object) : nullptr;
  return unwrapped && JS::GetClass(unwrapped) == &kVTTRegionClass;
}
bool InstallVTTRegion(State& state) {
  JS::RootedObject object(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!state.kit.Define("VTTRegion", &kVTTRegionClass, object, Constructor, &state.vttRegion)) return false;
  for (int i = 0; i < RegionSlotCount; ++i)
    if (!state.kit.Accessor(state.vttRegion, "VTTRegion", kFields[i], Get, Set, i)) return false;
  return MediaConstructorLast(state, state.vttRegion);
}
}
