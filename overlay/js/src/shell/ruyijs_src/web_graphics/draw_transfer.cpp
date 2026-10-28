// The transfer half of the draw path: the buffer contents, the attribute
// pointers, the uniform values, drawArrays and readPixels. The drawing buffer
// itself belongs to draw.cpp; the canvas handoff to surface.cpp.
#include "web_graphics/draw_calls.h"

#include <algorithm>
#include <cstring>

#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/ValueArray.h"
#include "js/experimental/TypedData.h"
#include "web_graphics/internal.h"
#include "web_graphics/public_calls.h"
#include "web_graphics/realm.h"

namespace ruyijs::web_graphics {
using namespace draw_call_detail;
namespace {
ReadPixelsFilter gReadPixelsFilter = nullptr;
bool InvalidOperation(Context& context) {
  detail::SetError(context, kInvalidOperation);
  return true;
}
bool InvalidEnum(Context& context) {
  detail::SetError(context, kInvalidEnum);
  return true;
}
}  // namespace

bool TransferOperation(JSContext* cx, const JS::CallArgs& args, State& state, Context& context,
                       int op) {
  args.rval().setUndefined();
  switch (op) {
    case kBufferData: {
      int32_t target = 0;
      if (!detail::ToEnum(cx, args.get(0), &target)) return false;
      if (target != kArrayBuffer && target != kElementArrayBuffer) return InvalidEnum(context);
      if (args.get(1).isNull() || args.get(1).isUndefined()) {
        detail::SetError(context, kInvalidValue);
        return true;
      }
      JS::PersistentRootedObject* binding =
          target == kArrayBuffer ? &context.arrayBinding : &context.elementBinding;
      if (!args.get(1).isObject() || !*binding) return InvalidOperation(context);
      JS::RootedObject source(cx, &args.get(1).toObject());
      const size_t length = JS_GetTypedArrayByteLength(source);
      JS::RootedObject copy(cx, JS_NewUint8Array(cx, length ? length : 1));
      if (!copy) return false;
      {
        JS::AutoCheckCannotGC nogc;
        bool shared = false;
        const uint8_t* bytes = static_cast<const uint8_t*>(
            JS_GetArrayBufferViewData(source, &shared, nogc));
        uint8_t* target8 = static_cast<uint8_t*>(JS_GetArrayBufferViewData(copy, &shared, nogc));
        if (!bytes || !target8) return InvalidOperation(context);
        if (length) std::memcpy(target8, bytes, length);
      }
      JS::SetReservedSlot(*binding, detail::kObjectDataSlot, JS::ObjectValue(*copy));
      return true;
    }
    case kEnableVertexAttrib: case kDisableVertexAttrib: {
      int32_t index = 0;
      if (!JS::ToInt32(cx, args.get(0), &index)) return false;
      if (index < 0 || index >= int32_t(context.attributeArrays.size()))
        return InvalidOperation(context);
      context.attributeArrays[size_t(index)].enabled = op == kEnableVertexAttrib;
      return true;
    }
    case kVertexAttribPointer: {
      int32_t index = 0, size = 0, type = 0, stride = 0;
      double offset = 0.0;
      if (!JS::ToInt32(cx, args.get(0), &index) || !JS::ToInt32(cx, args.get(1), &size) ||
          !detail::ToEnum(cx, args.get(2), &type) || !JS::ToInt32(cx, args.get(4), &stride) ||
          !JS::ToNumber(cx, args.get(5), &offset)) return false;
      if (index < 0 || index >= int32_t(context.attributeArrays.size()) || size < 1 || size > 4 ||
          stride < 0 || offset < 0.0) return InvalidOperation(context);
      // FLOAT is the only component type the attribute reader decodes.
      if (type != 0x1406) return InvalidEnum(context);
      AttributeArray& pointer = context.attributeArrays[size_t(index)];
      pointer.size = size;
      pointer.type = type;
      pointer.normalized = JS::ToBoolean(args.get(3));
      // A zero stride means the array is tightly packed: size components each.
      pointer.stride = stride > 0 ? stride : size * 4;
      pointer.offset = int32_t(offset);
      return true;
    }
    case kUniformFloat: {
      const JSObject* location = args.get(0).isObject() ? &args.get(0).toObject() : nullptr;
      if (!location || JS::GetClass(location) != &kUniformLocationClass ||
          detail::ObjectContext(const_cast<JSObject*>(location)) != &context)
        return InvalidOperation(context);
      JS::RootedValue stored(cx, JS::GetReservedSlot(const_cast<JSObject*>(location),
                                                    detail::kObjectDataSlot));
      if (!stored.isString()) return InvalidOperation(context);
      std::string name;
      if (!TextOf(cx, stored, &name)) return false;
      UniformSlot* slot = nullptr;
      for (UniformSlot& entry : context.uniforms)
        if (entry.name == name) slot = &entry;
      if (!slot) {
        context.uniforms.emplace_back();
        slot = &context.uniforms.back();
        slot->name = name;
      }
      slot->count = 0;
      slot->set = true;
      for (int32_t i = 0; i < 4; ++i) slot->value[i] = 0.0f;
      for (unsigned index = 1; index < args.length() && index < 5; ++index) {
        double number = 0.0;
        if (!JS::ToNumber(cx, args.get(index), &number)) return false;
        slot->value[index - 1] = float(number);
        ++slot->count;
      }
      return true;
    }
    case kDrawArrays: {
      if (context.drawFramebuffer) { detail::SetError(context, 0x0506); return true; }
      int32_t mode = 0, first = 0, count = 0;
      if (!detail::ToEnum(cx, args.get(0), &mode) || !JS::ToInt32(cx, args.get(1), &first) ||
          !JS::ToInt32(cx, args.get(2), &count)) return false;
      if (count < 0 || first < 0) return InvalidOperation(context);
      // TRIANGLE_STRIP is the only primitive this rasterizer draws; the other
      // modes stay a no-op rather than an error, as drawArrays is valid for them.
      if (mode == 0x0005) {
        int32_t width = 300, height = 150;
        CanvasSize(cx, context, &width, &height);
        ResizeDrawingBuffer(context, width, height);
        if (!DrawTriangleStrip(state, context, first, count)) return false;
      }
      return SyncCanvasSurface(state, context);
    }
    case kReadPixels: {
      if (context.readFramebuffer) { detail::SetError(context, 0x0506); return true; }
      int32_t x = 0, y = 0, width = 0, height = 0;
      if (!JS::ToInt32(cx, args.get(0), &x) || !JS::ToInt32(cx, args.get(1), &y) ||
          !JS::ToInt32(cx, args.get(2), &width) || !JS::ToInt32(cx, args.get(3), &height))
        return false;
      int32_t format = 0, type = 0;
      if (!detail::ToEnum(cx, args.get(4), &format) || !detail::ToEnum(cx, args.get(5), &type))
        return false;
      if (format != 0x1908 || type != 0x1401) return InvalidOperation(context);
      if (width < 0 || height < 0) return InvalidOperation(context);
      if (!args.get(6).isObject()) return true;
      JS::RootedObject pixels(cx, &args.get(6).toObject());
      if (JS_GetArrayBufferViewType(pixels) != JS::Scalar::Uint8) return InvalidOperation(context);
      int32_t canvasWidth = 300, canvasHeight = 150;
      CanvasSize(cx, context, &canvasWidth, &canvasHeight);
      ResizeDrawingBuffer(context, canvasWidth, canvasHeight);
      const size_t available = JS_GetTypedArrayByteLength(pixels);
      canvasWidth = context.drawingWidth;
      canvasHeight = context.drawingHeight;
      JS::AutoCheckCannotGC nogc;
      bool shared = false;
      uint8_t* target = static_cast<uint8_t*>(JS_GetArrayBufferViewData(pixels, &shared, nogc));
      if (!target) return true;
      for (int32_t row = 0; row < height; ++row) {
        const int32_t source = y + row;
        if (source < 0 || source >= canvasHeight) continue;
        for (int32_t column = 0; column < width; ++column) {
          const int32_t from = x + column;
          if (from < 0 || from >= canvasWidth) continue;
          const size_t destination = (size_t(row) * size_t(width) + size_t(column)) * 4u;
          if (destination + 4u > available) continue;
          const uint8_t* pixel = context.drawingBuffer.data() +
                                 (size_t(source) * size_t(canvasWidth) + size_t(from)) * 4u;
          std::memcpy(target + destination, pixel, 4);
        }
      }
      // PerturbWebGLReadPixelsRGBA8: only a read wholly inside the drawing
      // buffer's rows, origin flipped to the top-down image row, y upward.
      if (gReadPixelsFilter && width > 0 && height > 0 && x >= 0 && y >= 0 &&
          int64_t(y) + height <= canvasHeight &&
          uint64_t(width) * uint64_t(height) * 4u <= available)
        gReadPixelsFilter(target, uint32_t(width), uint32_t(height), width * 4, x,
                          canvasHeight - 1 - y, -1);
      return true;
    }
    default:
      return true;
  }
}
void SetReadPixelsFilter(ReadPixelsFilter filter) { gReadPixelsFilter = filter; }
}  // namespace ruyijs::web_graphics
