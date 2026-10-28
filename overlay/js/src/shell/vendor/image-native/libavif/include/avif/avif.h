/* ruyi-clone stub: libavif API surface used by web_media/image_avif.cpp.
   Types are complete enough to compile; every decode entry point fails,
   so AVIF images degrade to InvalidData until the real vendor lib is
   linked (PNG/GIF/WebP/APNG have in-tree decoders and are unaffected). */
#ifndef AVIF_STUB_H
#define AVIF_STUB_H
#include <stddef.h>
#include <stdint.h>

typedef enum { AVIF_RESULT_OK = 0, AVIF_RESULT_NO_CONTENT = 1,
               AVIF_RESULT_UNKNOWN_ERROR = 99 } avifResult;
typedef enum { AVIF_CODEC_CHOICE_AUTO = 0, AVIF_CODEC_CHOICE_DAV1D = 1,
               AVIF_CODEC_CHOICE_AOM = 2 } avifCodecChoice;
typedef enum { AVIF_RGB_FORMAT_RGBA = 0, AVIF_RGB_FORMAT_ARGB,
               AVIF_RGB_FORMAT_BGR, AVIF_RGB_FORMAT_BGRA,
               AVIF_RGB_FORMAT_ABGR, AVIF_RGB_FORMAT_RGB_565,
               AVIF_RGB_FORMAT_RGBA_1010102 } avifRGBFormat;
typedef enum { AVIF_FALSE = 0, AVIF_TRUE = 1 } avifBool;

typedef struct avifImage {
  uint32_t width, height, depth;
  uint8_t* alphaPlane;
  uint32_t alphaRowBytes;
} avifImage;

typedef struct avifRWData {
  uint8_t* data;
  size_t size;
} avifRWData;

typedef struct avifDiagnostics {
  char error[256];
} avifDiagnostics;

typedef struct avifImageTiming {
  double pts;
  double duration;
  uint64_t timescale;
} avifImageTiming;

typedef struct avifDecoder {
  int maxThreads;
  uint32_t imageSizeLimit;
  int imageCountLimit;
  avifCodecChoice codecChoice;
  int imageCount;
  int repetitionCount;
  int direction;
  uint32_t width, height, depth;
  avifImage* image;
  avifDiagnostics diag;
  avifImageTiming imageTiming;
} avifDecoder;

typedef struct avifRGBImage {
  uint32_t width, height, depth;
  avifRGBFormat format;
  avifBool alphaPremultiplied;
  uint8_t* pixels;
  uint32_t rowBytes;
} avifRGBImage;

#ifdef __cplusplus
extern "C" {
#endif

inline avifDecoder* avifDecoderCreate(void) { return new avifDecoder{}; }
inline void avifDecoderDestroy(avifDecoder* d) { delete d; }
inline avifResult avifDecoderSetIOMemory(avifDecoder*, const uint8_t*, size_t) {
  return AVIF_RESULT_UNKNOWN_ERROR;
}
inline avifResult avifDecoderParse(avifDecoder*) {
  return AVIF_RESULT_UNKNOWN_ERROR;
}
inline avifResult avifDecoderNextImage(avifDecoder*) {
  return AVIF_RESULT_UNKNOWN_ERROR;
}
inline avifResult avifImageScale(const avifImage*, uint32_t, uint32_t,
                                 avifDiagnostics*) {
  return AVIF_RESULT_UNKNOWN_ERROR;
}
inline avifResult avifImageYUVToRGB(const avifImage*, const avifRGBImage*) {
  return AVIF_RESULT_UNKNOWN_ERROR;
}
inline void avifRGBImageSetDefaults(avifRGBImage* rgb, const avifImage* image) {
  rgb->width = image->width;
  rgb->height = image->height;
  rgb->depth = 8;
  rgb->format = AVIF_RGB_FORMAT_RGBA;
  rgb->alphaPremultiplied = AVIF_FALSE;
  rgb->pixels = nullptr;
  rgb->rowBytes = 0;
}

#ifdef __cplusplus
}
#endif
#endif
