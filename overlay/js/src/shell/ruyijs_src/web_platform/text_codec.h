#ifndef RUYIJS_WEB_PLATFORM_TEXT_CODEC_H
#define RUYIJS_WEB_PLATFORM_TEXT_CODEC_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ruyijs::web_platform {

// One bounded ceiling for every text conversion in the domain, aligned with the
// resource protocol's body boundary. A conversion that would grow past it is
// refused with an explicit error instead of expanding without limit.
inline constexpr size_t kTextByteLimit = 8 * 1024 * 1024;

// The original Firefox encoding_rs library owns label resolution and decoding.
bool EncodingFromLabel(std::string_view label, std::string* encoding);

// Appends the UTF-8 form of the code units to `out`; a lone surrogate becomes
// U+FFFD. Returns false once `out` would pass kTextByteLimit.
bool AppendUtf8(std::u16string_view units, std::string* out);

// Encodes whole code points into at most `capacity` bytes and reports how many
// code units were consumed and how many bytes were written.
void EncodeUtf8Into(std::u16string_view units, uint8_t* out, size_t capacity,
                    size_t* read, size_t* written);

struct DecodeResult {
  std::u16string text;
  bool error = false;
};

// Opaque infrastructure codec, no JS or public ABI types leak to consumers.
void* OpenTextDecoder(std::string_view label, bool ignore_bom);
void CloseTextDecoder(void* decoder);
enum class DecodeStatus { Ok, Invalid, Limit, OutOfMemory };
DecodeStatus CopyDecoderBytes(const char*,size_t,std::string*) noexcept;
DecodeStatus DecoderEncodingFromLabel(std::string_view,std::string*) noexcept;
DecodeStatus DecodeText(void* decoder, std::string_view bytes, bool stream,
                        bool fatal, bool ignore_bom, std::u16string* text) noexcept;
DecodeResult DecodeBytes(std::string_view encoding, std::string_view bytes);
}  // namespace ruyijs::web_platform
#endif
