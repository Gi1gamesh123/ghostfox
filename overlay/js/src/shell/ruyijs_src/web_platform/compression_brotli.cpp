// The donor's original Brotli defaults and PROCESS/FINISH operations.
#include "web_platform/compression_codec.h"
#include "../../vendor/compression-native/brotli/include/brotli/encode.h"
#include "../../vendor/compression-native/brotli/include/brotli/decode.h"

namespace ruyijs::web_platform {
void* OpenBrotliCodec(bool decompress) {
  return decompress?static_cast<void*>(BrotliDecoderCreateInstance(nullptr,nullptr,nullptr)):
      static_cast<void*>(BrotliEncoderCreateInstance(nullptr,nullptr,nullptr));
}
void CloseBrotliCodec(void* handle,bool decompress) {
  if (decompress) BrotliDecoderDestroyInstance(static_cast<BrotliDecoderState*>(handle));
  else BrotliEncoderDestroyInstance(static_cast<BrotliEncoderState*>(handle));
}
CompressionStep StepBrotli(CompressionCodec& codec,const uint8_t* input,size_t length,
    uint8_t* output,size_t capacity,bool flush) {
  CompressionStep result;size_t available=length,space=capacity;
  if (codec.decompress) {
    auto* state=static_cast<BrotliDecoderState*>(codec.handle);
    const auto status=BrotliDecoderDecompressStream(state,&available,&input,&space,&output,nullptr);
    if (status==BROTLI_DECODER_RESULT_ERROR) {
      result.error="Brotli decompression error: {0}";
      result.detail=BrotliDecoderErrorString(BrotliDecoderGetErrorCode(state));
    } else if (status==BROTLI_DECODER_RESULT_SUCCESS) codec.ended=true;
    result.again=BrotliDecoderHasMoreOutput(state);
  } else {
    auto* state=static_cast<BrotliEncoderState*>(codec.handle);
    if (!BrotliEncoderCompressStream(state,flush?BROTLI_OPERATION_FINISH:BROTLI_OPERATION_PROCESS,
        &available,&input,&space,&output,nullptr)) result.error="Unexpected compression error";
    result.again=BrotliEncoderHasMoreOutput(state);
  }
  result.consumed=length-available;result.written=capacity-space;return result;
}
}
