// GIF frames returned by WIC are frame rectangles, so compose onto the canvas.
#include "web_media/image_wic.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace ruyijs::web_media::wic {
bool GifFrames(IWICImagingFactory* factory, IWICBitmapDecoder* decoder,
               uint32_t desiredWidth, uint32_t desiredHeight,
               domain::web_media::ImageData* out) {
  UINT count = 0;
  Pointer<IWICMetadataQueryReader> metadata;
  if (FAILED(decoder->GetFrameCount(&count)) || FAILED(decoder->GetMetadataQueryReader(metadata.put()))) return false;
  const uint32_t width = Number(metadata, L"/logscrdesc/Width"), height = Number(metadata, L"/logscrdesc/Height");
  if (!width || !height || uint64_t(width) * height * count > domain::web_media::kImagePixelLimit) return false;
  out->animated = true; out->repetitions = 0;
  PROPVARIANT loop; PropVariantInit(&loop);
  if (SUCCEEDED(metadata->GetMetadataByName(L"/appext/data", &loop))) {
    if (loop.vt == (VT_VECTOR | VT_UI1) && loop.caub.cElems >= 4 && loop.caub.pElems[1] == 1) {
      const unsigned repeats = unsigned(loop.caub.pElems[2]) | unsigned(loop.caub.pElems[3]) << 8;
      out->repetitions = repeats ? repeats : std::numeric_limits<double>::infinity();
    }
    PropVariantClear(&loop);
  }
  std::vector<uint8_t> canvas(size_t(width) * height * 4, 0), previous;
  uint64_t timestamp = 0;
  for (UINT index = 0; index < count; ++index) {
    Pointer<IWICBitmapFrameDecode> frame;
    Pointer<IWICMetadataQueryReader> info;
    domain::web_media::ImageFrame patch;
    if (FAILED(decoder->GetFrame(index, frame.put())) ||
        FAILED(frame->GetMetadataQueryReader(info.put())) || !PixelFrame(factory, frame, 0, 0, &patch)) return false;
    const uint32_t x = Number(info, L"/imgdesc/Left"), y = Number(info, L"/imgdesc/Top");
    const uint32_t disposal = Number(info, L"/grctlext/Disposal");
    if (disposal == 3) previous = canvas;
    for (uint32_t row = 0; row < patch.height && uint64_t(y) + row < height; ++row)
      for (uint32_t col = 0; col < patch.width && uint64_t(x) + col < width; ++col) {
        const uint8_t* pixel = &patch.pixels[(size_t(row) * patch.width + col) * 4];
        if (pixel[3]) std::memcpy(&canvas[(size_t(y + row) * width + x + col) * 4], pixel, 4);
      }
    domain::web_media::ImageFrame complete;
    complete.width = width; complete.height = height; complete.pixels = canvas;
    complete.animated = true; complete.timestamp = timestamp;
    uint32_t delay = Number(info, L"/grctlext/Delay") * 10000;
    complete.duration = delay < 20000 ? 100000 : delay; timestamp += complete.duration;
    complete.opaque = false;  // The animation compositing surface has an alpha plane.
    if (desiredWidth && desiredHeight && (desiredWidth != width || desiredHeight != height)) {
      if (uint64_t(desiredWidth) * desiredHeight > domain::web_media::kImagePixelLimit) return false;
      std::vector<uint8_t> resized(size_t(desiredWidth) * desiredHeight * 4);
      for (uint32_t row = 0; row < desiredHeight; ++row) for (uint32_t col = 0; col < desiredWidth; ++col)
        std::memcpy(&resized[(size_t(row) * desiredWidth + col) * 4],
            &canvas[(size_t(uint64_t(row) * height / desiredHeight) * width + uint64_t(col) * width / desiredWidth) * 4], 4);
      complete.width = desiredWidth; complete.height = desiredHeight; complete.pixels = std::move(resized);
    }
    out->frames.push_back(std::move(complete));
    if (disposal == 2) {
      for (uint32_t row = 0; row < patch.height && uint64_t(y) + row < height; ++row)
        for (uint32_t col = 0; col < patch.width && uint64_t(x) + col < width; ++col)
          std::memset(&canvas[(size_t(y + row) * width + x + col) * 4], 0, 4);
    } else if (disposal == 3) canvas.swap(previous);
  }
  return true;
}
}
