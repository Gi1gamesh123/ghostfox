// The FileReader read methods and abort. The byte work happens now (the source
// Blob already holds bounded bytes) and the computed value is handed to one job of
// the engine job queue, so result/readyState and the load event land after the
// calling script returns. The generation counter makes an aborted or replaced read
// a no-op when its job finally runs.
#include "web_platform/state.h"
#include "web_platform/text_codec.h"

#include "interaction/realm.h"
#include "runtime/resource_buffer.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/ArrayBuffer.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/experimental/TypedData.h"
#include <cstring>
#include <string>

namespace ruyijs::web_platform {
namespace {
bool BlobBytesOf(JSContext* cx, JS::HandleValue value, std::string* bytes) {
  if (!IsBlob(value)) return WebKit::Fail(cx, "FileReader read requires a Blob");
  const uint8_t* data = nullptr;
  size_t length = 0;
  JS::RootedObject blob(cx, &value.toObject());
  if (!BlobBytes(cx, blob, &data, &length)) return false;
  bytes->assign(reinterpret_cast<const char*>(data), length);
  return true;
}

bool StringValue(JSContext* cx, std::u16string_view units, JS::MutableHandleValue out) {
  JS::RootedString text(cx, JS_NewUCStringCopyN(cx, units.data(), units.size()));
  if (!text) return false;
  out.setString(text);
  return true;
}

// readAsText: the optional label uses TextDecoder's Firefox encoding resolver,
// so an unsupported label fails the read instead of silently reinterpreting bytes.
bool TextResult(State& state, const std::string& bytes, JS::HandleValue label,
                JS::MutableHandleValue out, bool* failed) {
  JSContext* cx = state.cx;
  *failed = false;
  std::string encoding = "utf-8";
  if (!label.isUndefined()) {
    std::string requested;
    if (!Convert(cx, label, &requested)) return false;
    if (!EncodingFromLabel(requested, &encoding)) {
      *failed = true;
      return MakeException(state, "EncodingError", "FileReader does not support this encoding label",
                           out);
    }
  }
  const DecodeResult decoded = DecodeBytes(encoding, bytes);
  if (decoded.error) { JS_ReportOutOfMemory(cx); return false; }
  return StringValue(cx, decoded.text, out);
}

bool BufferResult(State& state, const std::string& bytes, JS::MutableHandleValue out) {
  JSContext* cx = state.cx;
  JS::RootedObject buffer(cx, JS::NewArrayBuffer(cx, bytes.size()));
  if (!buffer) return false;
  if (!bytes.empty()) {
    size_t length = 0;
    bool shared = false;
    uint8_t* data = nullptr;
    JS::GetArrayBufferLengthAndData(buffer, &length, &shared, &data);
    if (!data) return WebKit::Fail(cx, "FileReader produced a detached buffer");
    std::memcpy(data, bytes.data(), bytes.size());
  }
  out.setObject(*buffer);
  return true;
}

bool BinaryResult(JSContext* cx, const std::string& bytes, JS::MutableHandleValue out) {
  std::u16string units(bytes.size(), u'\0');
  for (size_t index = 0; index < bytes.size(); ++index)
    units[index] = char16_t(static_cast<unsigned char>(bytes[index]));
  return StringValue(cx, units, out);
}

bool DataUrlResult(State& state, JS::HandleObject blob, const std::string& bytes,
                   JS::MutableHandleValue out) {
  JSContext* cx = state.cx;
  JS::RootedValue type_value(cx, JS::GetReservedSlot(blob, kBlobTypeSlot));
  std::string type;
  if (!Convert(cx, type_value, &type)) return false;
  std::string encoded;
  const auto status = resource_wire::EncodeBase64(bytes, &encoded);
  if (status.code != resource_network::ErrorCode::None)
    return WebKit::FailKind(cx, status.message, JSEXN_RANGEERR);
  std::string url = "data:" + (type.empty() ? std::string("application/octet-stream") : type) +
                    ";base64," + encoded;
  std::u16string units(url.begin(), url.end());
  return StringValue(cx, units, out);
}

// abort(): only a loading read is cancelled; the generation bump drops the queued
// job, and abort/loadend fire from here.
bool Abort(State& state, JS::HandleObject self, JS::CallArgs& args) {
  JSContext* cx = state.cx;
  if (JS::GetReservedSlot(self, kReaderStateSlot).toInt32() == kReaderReadyLoading) {
    JS::SetReservedSlot(self, kReaderStateSlot, JS::Int32Value(kReaderReadyDone));
    JS::SetReservedSlot(self, kReaderResultSlot, JS::NullValue());
    JS::SetReservedSlot(self, kReaderGenerationSlot,
                        JS::Int32Value(JS::GetReservedSlot(self, kReaderGenerationSlot).toInt32() + 1));
    JS::RootedValue error(cx);
    if (!MakeException(state, "AbortError", "The read operation was aborted.", &error)) return false;
    JS::SetReservedSlot(self, kReaderErrorSlot, error);
    if (!state.events->Fire(self, "abort", false, false) ||
        !state.events->Fire(self, "loadend", false, false))
      return false;
  } else {
    JS::SetReservedSlot(self, kReaderResultSlot, JS::NullValue());
  }
  args.rval().setUndefined();
  return true;
}
}  // namespace

bool ReadBlobPayload(State& state, ReaderMethodKind kind, JS::HandleValue value, JS::HandleValue label,
                     JS::MutableHandleValue payload, bool* failed) {
  JSContext* cx = state.cx;
  *failed = false;
  std::string bytes;
  if (!BlobBytesOf(cx, value, &bytes)) return false;
  JS::RootedObject blob(cx, &value.toObject());
  switch (kind) {
    case kReaderText: return TextResult(state, bytes, label, payload, failed);
    case kReaderArrayBuffer: return BufferResult(state, bytes, payload);
    case kReaderDataURL: return DataUrlResult(state, blob, bytes, payload);
    default: return BinaryResult(cx, bytes, payload);
  }
}

bool FileReaderMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!ReaderBrand(cx, args.thisv(), &self)) return false;
  const ReaderMethodKind kind = ReaderMethodKind(WebKit::Slot(args));
  if (kind == kReaderAbort) return Abort(state, self, args);
  if (JS::GetReservedSlot(self, kReaderStateSlot).toInt32() == kReaderReadyLoading) {
    JS::RootedValue error(cx);
    if (!MakeException(state, "InvalidStateError", "The FileReader is already loading", &error))
      return false;
    JS_SetPendingException(cx, error);
    return false;
  }
  JS::RootedValue payload(cx);
  bool failed = false;
  if (!ReadBlobPayload(state, kind, args.get(0), args.get(1), &payload, &failed)) return false;
  JS::SetReservedSlot(self, kReaderStateSlot, JS::Int32Value(kReaderReadyLoading));
  JS::SetReservedSlot(self, kReaderResultSlot, JS::NullValue());
  JS::SetReservedSlot(self, kReaderErrorSlot, JS::NullValue());
  const int32_t generation =
      JS::GetReservedSlot(self, kReaderGenerationSlot).toInt32() + 1;
  JS::SetReservedSlot(self, kReaderGenerationSlot, JS::Int32Value(generation));
  if (!state.events->Fire(self, "loadstart", false, false)) return false;
  args.rval().setUndefined();
  return QueueReaderJob(state, self, generation, failed, payload);
}
}  // namespace ruyijs::web_platform
