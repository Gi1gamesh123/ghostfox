// Infrastructure adapter to the original Firefox encoding_rs codec. No JS API,
// label table or duplicate decoding state lives here. The donor owns algorithms.
#include "web_platform/text_codec.h"
#include <new>
#include <stdexcept>
struct Encoding;
struct Decoder;
struct Encoder;
#include "web_platform/encoding_rs/encoding_rs.h"

namespace ruyijs::web_platform {
namespace {
const uint8_t* Input(std::string_view bytes) {
  static constexpr uint8_t empty = 0;
  return bytes.empty() ? &empty : reinterpret_cast<const uint8_t*>(bytes.data());
}
const Encoding* Resolve(std::string_view label) {
  return encoding_for_label_no_replacement(Input(label), label.size());
}
}
bool EncodingFromLabel(std::string_view label, std::string* canonical) {
  const Encoding* encoding = Resolve(label);
  if (!encoding) return false;
  uint8_t name[ENCODING_NAME_MAX_LENGTH];
  const size_t length = encoding_name(encoding, name);
  canonical->resize(length);
  for (size_t i = 0; i < length; ++i)
    (*canonical)[i] = char(name[i] >= 'A' && name[i] <= 'Z' ? name[i] + 32 : name[i]);
  return true;
}
void* OpenTextDecoder(std::string_view label, bool ignore_bom) {
  const Encoding* encoding = Resolve(label);
  if (!encoding) return nullptr;
  return ignore_bom ? encoding_new_decoder_without_bom_handling(encoding)
                    : encoding_new_decoder_with_bom_removal(encoding);
}
void CloseTextDecoder(void* decoder) {
  if (decoder) decoder_free(static_cast<Decoder*>(decoder));
}
DecodeStatus DecodeText(void* opaque, std::string_view bytes, bool stream,
                        bool fatal, bool ignore_bom, std::u16string* text) noexcept {
  if (bytes.size() > kTextByteLimit) return DecodeStatus::Limit;
  auto* decoder = static_cast<Decoder*>(opaque);
  const size_t capacity = decoder_max_utf16_buffer_length(decoder, bytes.size());
  if (capacity > kTextByteLimit + 16) return DecodeStatus::Limit;
  try {
    text->resize(capacity);
    size_t read = bytes.size(), written = capacity;
    char16_t empty = 0;
    auto* output = capacity ? text->data() : &empty;
    bool replaced = false;
    const uint32_t status = fatal
        ? decoder_decode_to_utf16_without_replacement(decoder, Input(bytes), &read, output, &written, !stream)
        : decoder_decode_to_utf16(decoder, Input(bytes), &read, output, &written, !stream, &replaced);
    if (status != INPUT_EMPTY) { text->clear(); return DecodeStatus::Invalid; }
    text->resize(written);
    if (!stream) {
      const Encoding* encoding = decoder_encoding(decoder);
      if (ignore_bom) encoding_new_decoder_without_bom_handling_into(encoding, decoder);
      else encoding_new_decoder_with_bom_removal_into(encoding, decoder);
    }
    return DecodeStatus::Ok;
  } catch (const std::bad_alloc&) { return DecodeStatus::OutOfMemory; }
    catch (const std::length_error&) { return DecodeStatus::Limit; }
}
DecodeResult DecodeBytes(std::string_view encoding, std::string_view bytes) {
  DecodeResult result;
  void* decoder = OpenTextDecoder(encoding, true);
  if (!decoder) { result.error = true; return result; }
  result.error = DecodeText(decoder, bytes, false, false, true, &result.text) != DecodeStatus::Ok;
  CloseTextDecoder(decoder);
  return result;
}
DecodeStatus CopyDecoderBytes(const char* bytes,size_t length,std::string* output) noexcept {
  if (length>kTextByteLimit) return DecodeStatus::Limit;
  try {if (length) output->assign(bytes,length);else output->clear();return DecodeStatus::Ok;}
  catch (...) {return DecodeStatus::OutOfMemory;}
}
DecodeStatus DecoderEncodingFromLabel(std::string_view label,std::string* output) noexcept {
  try {return EncodingFromLabel(label,output)?DecodeStatus::Ok:DecodeStatus::Invalid;}
  catch (...) {return DecodeStatus::OutOfMemory;}
}

}
