#pragma once
#include "domain/web_media/image_data.h"

namespace ruyijs::web_media {
bool ImageCodecSupports(const std::string&);
void ImageColorTransform(domain::web_media::ImageFrame*);
domain::web_media::ImageDecodeStatus DecodeAvifBytes(const std::vector<uint8_t>&,
    uint32_t, uint32_t, domain::web_media::ImageData*);
domain::web_media::ImageDecodeStatus DecodeImageBytes(
    const std::string&, const std::vector<uint8_t>&, uint32_t, uint32_t,
    bool, domain::web_media::ImageData*);
}
