// The reflected width/height members of HTMLCanvasElement. The Document Model
// owns the element and its prototype; this domain installs only these
// descriptors and reads and writes the content attributes through the element's
// own DOM methods, so the attribute stays the single source of truth.
#include "web_canvas/internal.h"

namespace ruyijs::web_canvas {
bool CanvasWidthGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::CanvasReceiver(state, args.thisv(), &self, &args)) return false;
  uint32_t width = 0, height = 0;
  if (!detail::CanvasDimensions(state, self, &width, &height)) return false;
  args.rval().setNumber(double(width));
  return true;
}

bool CanvasHeightGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::CanvasReceiver(state, args.thisv(), &self, &args)) return false;
  uint32_t width = 0, height = 0;
  if (!detail::CanvasDimensions(state, self, &width, &height)) return false;
  args.rval().setNumber(double(height));
  return true;
}

// The reflected setter serializes the WebIDL unsigned long into the attribute.
// The bitmap reset follows from the attribute change notice (Realm::DimensionsChanged),
// the same path setAttribute and removeAttribute take.
bool CanvasWidthSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::CanvasReceiver(state, args.thisv(), &self, &args)) return false;
  args.rval().setUndefined();
  uint32_t value = 0;
  if (!detail::ToUnsignedLong(cx, args.get(0), &value)) return false;
  JS::RootedString text(cx);
  if (!detail::DecimalString(cx, value, &text) ||
      !detail::SetAttribute(state, self, "width", text)) return false;
  return true;
}

bool CanvasHeightSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::CanvasReceiver(state, args.thisv(), &self, &args)) return false;
  args.rval().setUndefined();
  uint32_t value = 0;
  if (!detail::ToUnsignedLong(cx, args.get(0), &value)) return false;
  JS::RootedString text(cx);
  if (!detail::DecimalString(cx, value, &text) ||
      !detail::SetAttribute(state, self, "height", text)) return false;
  return true;
}

bool InstallElement(State& state) {
  WebKit& kit = state.kit;
  JS::HandleObject prototype = state.canvasPrototype;
  return detail::AccessorOn(kit, prototype, "HTMLCanvasElement", "width", CanvasWidthGet,
                            CanvasWidthSet, 0) &&
         detail::AccessorOn(kit, prototype, "HTMLCanvasElement", "height", CanvasHeightGet,
                            CanvasHeightSet, 0);
}
}  // namespace ruyijs::web_canvas
