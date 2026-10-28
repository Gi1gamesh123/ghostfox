#include "web_audio/state.h"

#include "js/Array.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_audio {
namespace {

const char* kReceiver = "Web Audio member called on an incompatible receiver";

bool ChannelArray(JSContext* cx, JS::HandleObject self, uint32_t index,
                  JS::MutableHandleObject channel) {
  const int32_t channels = JS::GetReservedSlot(self, kBufferCountSlot).toInt32();
  if (index >= uint32_t(channels))
    return WebKit::FailKind(cx, "AudioBuffer channel index out of range", JSEXN_RANGEERR);
  JS::RootedObject data(cx, &JS::GetReservedSlot(self, kBufferDataSlot).toObject());
  JS::RootedValue element(cx);
  if (!JS_GetElement(cx, data, index, &element)) return false;
  channel.set(&element.toObject());
  return true;
}

// Copies element by element between the cached channel and an array-like
// argument, honouring the spec's separate source and destination offsets.
bool CopyElements(JSContext* cx, JS::HandleObject dest, uint32_t dest_offset,
                  JS::HandleObject source, uint32_t source_offset) {
  uint32_t source_length = 0;
  if (!JS::GetArrayLength(cx, source, &source_length)) return false;
  for (uint32_t index = source_offset; index < source_length; ++index) {
    JS::RootedValue element(cx);
    if (!JS_GetElement(cx, source, index, &element)) return false;
    if (!JS_DefineElement(cx, dest, dest_offset + (index - source_offset), element,
                          JSPROP_ENUMERATE)) return false;
  }
  return true;
}

}  // namespace

bool ChannelData(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kBufferClass, &self, kReceiver)) return false;
  double raw = 0.0;
  if (args.length() && !JS::ToNumber(cx, args[0], &raw)) return false;
  if (!(raw >= 0))
    return WebKit::FailKind(cx, "AudioBuffer channel index out of range", JSEXN_RANGEERR);
  JS::RootedObject channel(cx);
  if (!ChannelArray(cx, self, uint32_t(raw), &channel)) return false;
  args.rval().setObject(*channel);
  return true;
}

bool CopyChannel(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  // The native slot is read before rval(): accessing the return value makes
  // CallArgs::callee() assert, so the function data must be captured first.
  const int32_t operation = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kBufferClass, &self, kReceiver)) return false;
  args.rval().setUndefined();
  if (!args.length()) return true;
  double raw_channel = 0.0, raw_offset = 0.0;
  if (args.length() > 1 && !JS::ToNumber(cx, args[1], &raw_channel)) return false;
  if (args.length() > 2 && !JS::ToNumber(cx, args[2], &raw_offset)) return false;
  if (!(raw_channel >= 0) || !(raw_offset >= 0))
    return WebKit::FailKind(cx, "AudioBuffer channel range is invalid", JSEXN_RANGEERR);
  JS::RootedObject channel(cx);
  if (!ChannelArray(cx, self, uint32_t(raw_channel), &channel)) return false;
  if (!args[0].isObject())
    return WebKit::Fail(cx, "AudioBuffer channel data must be array-like");
  JS::RootedObject other(cx, &args[0].toObject());
  if (operation == 0)
    return CopyElements(cx, other, 0, channel, uint32_t(raw_offset));
  return CopyElements(cx, channel, uint32_t(raw_offset), other, 0);
}

}  // namespace ruyijs::web_audio
