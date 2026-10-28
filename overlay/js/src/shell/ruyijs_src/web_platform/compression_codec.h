#pragma once
#include <cstddef>
#include <cstdint>

namespace ruyijs::web_platform {
enum class CompressionFormat { Deflate,Raw,Gzip,Brotli };
struct CompressionCodec {
  CompressionFormat format;
  bool decompress=false,ended=false;
  void* handle=nullptr;
};
struct CompressionStep {
  size_t consumed=0,written=0;
  bool again=false;
  // Error prefixes are selected without allocating native strings.
  const char* error=nullptr;
  const char* detail=nullptr;
};
CompressionCodec* OpenCompressionCodec(CompressionFormat,bool);
void CloseCompressionCodec(CompressionCodec*);
CompressionStep StepZlib(CompressionCodec&,const uint8_t*,size_t,uint8_t*,size_t,bool);
CompressionStep StepBrotli(CompressionCodec&,const uint8_t*,size_t,uint8_t*,size_t,bool);
void* OpenZlibCodec(CompressionFormat,bool);
void CloseZlibCodec(void*,bool);
void* OpenBrotliCodec(bool);
void CloseBrotliCodec(void*,bool);
}
