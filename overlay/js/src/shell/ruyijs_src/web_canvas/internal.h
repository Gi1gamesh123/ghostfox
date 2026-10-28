#ifndef RUYIJS_WEB_CANVAS_INTERNAL_H
#define RUYIJS_WEB_CANVAS_INTERNAL_H
#include <cmath>
#include <cstdint>
#include <string>
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/experimental/TypedData.h"
#include "jsapi.h"
#include "web_canvas/state.h"

namespace ruyijs::web_canvas {
// The one pixel-area ceiling, in pixels: an 8 MiB RGBA store, the same order as
// the blob and text limits. A larger canvas has no pixels, so toDataURL returns
// "data:," instead of allocating.
inline constexpr uint64_t kCanvasAreaLimit = 2ull * 1024ull * 1024ull;
}  // namespace ruyijs::web_canvas

namespace ruyijs::web_geometry {
class SystemFonts;
}  // namespace ruyijs::web_geometry

namespace ruyijs::web_canvas::detail {
// Reads the install-time state from the function's reserved slot. Never call this
// after touching args.rval(): Owner() reads the callee and asserts on that.
inline State* Self(const JS::CallArgs& args) {
  return static_cast<State*>(WebKit::Owner(args));
}

// True when the value's prototype chain reaches this realm's HTMLCanvasElement
// prototype, which is how the Document Model's canvas elements are recognised
// without this domain depending on the DOM tree.
bool IsCanvas(State& state, JS::HandleValue value);

// Calls one of the receiver's own public methods. The DOM stays the single source
// of truth for the reflected width/height content attributes.
bool CallMethod(JSContext* cx, JS::HandleObject receiver, const char* name,
          JS::HandleValueArray args, JS::MutableHandleValue out);
bool GetAttribute(State& state, JS::HandleObject element, const char* name,
                  JS::MutableHandleValue out);
bool SetAttribute(State& state, JS::HandleObject element, const char* name,
                  JS::HandleString value);

// WebIDL long conversion, used by the pixel methods that take signed counts.
inline bool ToLong(JSContext* cx, JS::HandleValue value, int32_t* out) {
  return JS::ToInt32(cx, value, out);
}

// WebIDL unsigned long conversion: NaN, Infinity and zero become 0, other values
// are truncated toward zero and wrapped into [0, 2^32).
bool ToUnsignedLong(JSContext* cx, JS::HandleValue value, uint32_t* out);
// The HTML rules for parsing non-negative integers, restricted to an optional
// "+" followed by ASCII digits; everything else (including units) is invalid.
bool ParseNonNegative(JSContext* cx, JS::HandleString text, uint32_t* out);
// The reflected dimension: the content attribute parsed as a non-negative
// integer, or the fallback when it is absent or invalid.
bool CanvasDimension(State& state, JS::HandleObject element, const char* name,
                     uint32_t fallback, uint32_t* out);
// The same parse of an attribute value the Document Model already read
// (a string, or null when the attribute is absent).
bool ParseDimension(JSContext* cx, JS::HandleValue raw, uint32_t fallback, uint32_t* out);
// The serialized form the reflected setter writes into the content attribute.
bool DecimalString(JSContext* cx, uint32_t value, JS::MutableHandleString out);

// Case-sensitive and ASCII case-insensitive comparison with a literal.
bool StringEqualsAscii(JSContext* cx, JS::HandleString text, const char* ascii, bool* match);
// The ASCII bytes of a string; |out| stays empty when the string is not ASCII.
bool StringToAscii(JSContext* cx, JS::HandleString text, std::string* out);
bool StringEqualsAsciiCI(JSContext* cx, JS::HandleString text, const char* ascii, bool* match);

// The binding of one canvas, created on demand.
CanvasBinding* EnsureCanvas(State& state, JS::HandleObject canvas);
// Re-allocates the bitmap from the given dimensions; a size above
// kCanvasAreaLimit leaves it empty, which is what "no pixels" means here.
bool ResizeBitmap(State& state, CanvasBinding& binding, uint32_t width, uint32_t height);

// A fresh drawing state carrying the specification defaults.
JSObject* NewDrawingState(JSContext* cx, JS::MutableHandleObject out);
// Slot 0 of a context: the canvas it was created from.
JSObject* ContextCanvas(State& state, JS::HandleObject context);
// The binding that owns this context, or null when the canvas is gone.
CanvasBinding* BindingOf(State& state, JS::HandleObject context);
// The six matrix components of one drawing state.
bool StateMatrix(JSContext* cx, JS::HandleObject state, double out[6]);
// The one RGBA store of a canvas, and its bytes (empty when it has no pixels).
JSObject* BitmapOf(CanvasBinding* binding);
std::string PixelsOf(JSContext* cx, CanvasBinding* binding);

// The pixel array of an ImageData, and the raw bytes of a typed array view.
inline JSObject* ImagePixels(JSObject* image) {
  JS::Value slot = JS::GetReservedSlot(image, kImageDataSlot);
  return slot.isObject() ? &slot.toObject() : nullptr;
}
inline uint8_t* ViewBytes(JSObject* view) {
  bool shared = false;
  JS::AutoCheckCannotGC nogc;
  return static_cast<uint8_t*>(JS_GetArrayBufferViewData(view, &shared, nogc));
}

// font_parse.cpp: the parts of the ctx.font shorthand text measurement needs.
struct CanvasFont {
  std::string family;
  double size = 16;  // CSS pixels
  unsigned weight = 400;
  bool italic = false;
  bool valid = false;
};
bool ParseCanvasFont(const std::string& shorthand, CanvasFont* out);
// text_metrics.cpp: the realm's font owner (web_geometry), created on first use.
// It caches the system font collection only; nothing page-specific is stored.
web_geometry::SystemFonts& Fonts(State& state);
// The UTF-16 contents of a string, which the shaping owner measures.
bool StringUnits(JSContext* cx, JS::HandleString text, std::u16string* out);

// The PNG Firefox's encoder would write for these RGBA bytes. A non-empty
// metadata key appends nsPNGEncoder's "deBG" chunk after IDAT.
std::string EncodePNG(const uint8_t* pixels, uint32_t width, uint32_t height,
                      const std::string& metadataKey);
std::string Base64(const std::string& bytes);
// png_metadata.cpp: nsRFPService::GenerateRandomizationKeyFromHash, and this
// Context's one 32-byte key. The chunk is random per session in Firefox too, so
// only its presence, position, size and within-page stability are observable.
std::string PngMetadataHex(const std::string& key, const std::string& content);
const std::string& PngMetadataKey(State& state);
// The Context's configured canvas fingerprint (the donor --fpfile canvas.* keys);
// when enabled it replaces the session key: its derived key with pngMetadata,
// otherwise no chunk at all (CanvasRenderingContextHelper::ToBlob).
const std::string* ConfiguredMetadataKey();
// Firefox hands the session key to the encoder only when the extraction came
// from a context's image buffer (CanvasRenderingContextHelper::ToBlob,
// HTMLCanvasElement::ExtractData), so a canvas that never had a context encodes
// a chunk-less PNG. The bitmap renderer context takes the same buffer path.
inline const std::string& MetadataKeyFor(State& state, const CanvasBinding& binding) {
  static const std::string kNone;
  // A configured (fpfile) key wins regardless of the context, as in
  // CanvasRenderingContextHelper::ToBlob / HTMLCanvasElement::ExtractData.
  if (const std::string* configured = ConfiguredMetadataKey()) return *configured;
  if (!binding.context2d && !binding.bitmapContext) return kNone;
  return PngMetadataKey(state);
}

// Creates the page realm's own Blob, exactly like the fetch body readers do.
bool PageConstructBlob(State& state, const std::string& bytes, JS::HandleString type,
                       JS::MutableHandleObject out);
// Queues one job carrying value on the engine job queue through an already
// resolved promise, so a callback never runs synchronously.
bool QueueJob(State& state, JS::HandleValue callback, JS::HandleValue value);
// The current drawing state of a context and its save/restore stack.
JSObject* DrawingState(JSContext* cx, JS::HandleObject context);
bool SetDrawingState(JSContext* cx, JS::HandleObject context, JS::HandleObject state);
// The current transform and its mutation, in the canvas [a b c d e f] order.
bool GetMatrix(JSContext* cx, JS::HandleObject context, double components[6]);
bool SetMatrix(JSContext* cx, JS::HandleObject context, const double components[6]);
// The receiver check and argument conversions shared by the 2D-context member
// files, so the path and transform files each stay a single concern.
inline int32_t ContextSlot(const JS::CallArgs& args) { return WebKit::Slot(args) & 0xffff; }
inline bool ContextReceiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject out) {
  bool off = WebKit::Slot(args) & 0x10000;
  return WebKit::CheckBrand(cx, args, off ? &kOffscreen2DClass : &kContext2DClass, out,
                            off ? "OffscreenCanvasRenderingContext2D" : "CanvasRenderingContext2D");
}
inline bool Method2D(State& s, WebFace& f, const char* name, JSNative fn, unsigned n, int32_t op) {
  bool off = &f == &s.offscreen2d;
  return s.kit.Method(f, off ? "OffscreenCanvasRenderingContext2D" : "CanvasRenderingContext2D",
                      name, fn, n, op | (off ? 0x10000 : 0));
}
inline bool Accessor2D(State& s, WebFace& f, const char* name, JSNative get, JSNative set, int32_t op) {
  bool off = &f == &s.offscreen2d;
  return s.kit.Accessor(f, off ? "OffscreenCanvasRenderingContext2D" : "CanvasRenderingContext2D",
                        name, get, set, op | (off ? 0x10000 : 0));
}
inline bool Number(JSContext* cx, JS::HandleValue value, double* out, bool* finite) {
  double number = 0.0;
  if (!JS::ToNumber(cx, value, &number)) return false;
  *out = number;
  *finite = std::isfinite(number);
  return true;
}
// Reads `count` finite numbers from the arguments starting at `begin`. A
// non-finite component makes the whole call a no-op, as the specification says.
inline bool ReadNumbers(JSContext* cx, const JS::CallArgs& args, unsigned begin,
                        unsigned count, double* out, bool* usable) {
  for (unsigned index = 0; index < count; ++index) {
    bool finite = false;
    if (!Number(cx, args.get(begin + index), &out[index], &finite)) return false;
    if (!finite) { *usable = false; return true; }
  }
  *usable = true;
  return true;
}
// Registers a member on a prototype another owner already created. The adding
// domain owns exactly the descriptors it adds, and the temporary face only
// carries the target prototype. The prototype is a handle: every registration
// allocates, so a raw JSObject* held across the calls could be moved by a GC.
inline bool AccessorOn(WebKit& kit, JS::HandleObject prototype, const char* iface,
                       const char* name, JSNative getter, JSNative setter, int32_t slot) {
  WebFace face(kit.cx);
  face.prototype.set(prototype);
  return kit.Accessor(face, iface, name, getter, setter, slot);
}
inline bool MethodOn(WebKit& kit, JS::HandleObject prototype, const char* iface,
                     const char* name, JSNative native, unsigned length, int32_t slot) {
  WebFace face(kit.cx);
  face.prototype.set(prototype);
  return kit.Method(face, iface, name, native, length, slot);
}
// The canvas element receiver check, shared by the element member files.
inline bool CanvasReceiver(State& state, JS::HandleValue self, JS::MutableHandleObject out,
                           const JS::CallArgs* args = nullptr) {
  if (!IsCanvas(state, self)) {
    std::string text;
    if (args && WebKit::BrandText(state.cx, *args, "HTMLCanvasElement", &text))
      return WebKit::Fail(state.cx, text.c_str());
    return WebKit::Fail(state.cx, "Illegal invocation");
  }
  out.set(&self.toObject());
  return true;
}
// Both reflected dimensions, read from the content attributes.
inline bool CanvasDimensions(State& state, JS::HandleObject element, uint32_t* width,
                             uint32_t* height) {
  return CanvasDimension(state, element, "width", 300, width) &&
         CanvasDimension(state, element, "height", 150, height);
}
// A 2D context back to its default drawing state, empty state stack and path;
// CanvasRenderingContext2D.reset() and a dimension change share it.
bool ResetContext2D(State& state, JS::HandleObject context);
}  // namespace ruyijs::web_canvas::detail
#endif
