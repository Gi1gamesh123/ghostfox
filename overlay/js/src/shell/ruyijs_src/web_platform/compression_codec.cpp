// Native codec state has no JS roots or realm pointers.
#include "web_platform/compression_codec.h"
#include <new>

namespace ruyijs::web_platform {
CompressionCodec* OpenCompressionCodec(CompressionFormat format,bool decompress) {
  auto* codec=new(std::nothrow) CompressionCodec{format,decompress};
  if (!codec) return nullptr;
  codec->handle=format==CompressionFormat::Brotli?OpenBrotliCodec(decompress):OpenZlibCodec(format,decompress);
  if (!codec->handle) {delete codec;return nullptr;}
  return codec;
}
void CloseCompressionCodec(CompressionCodec* codec) {
  if (!codec) return;
  if (codec->format==CompressionFormat::Brotli) CloseBrotliCodec(codec->handle,codec->decompress);
  else CloseZlibCodec(codec->handle,codec->decompress);
  delete codec;
}
}
