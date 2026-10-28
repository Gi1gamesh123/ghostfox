#pragma once
#include <cstdint>
#include <vector>
namespace ruyijs::web_media {
struct GifMetadata {uint32_t frames=0;bool complete=false,animated=false;double repetitions=0;};
bool ReadGifMetadata(const std::vector<uint8_t>&,GifMetadata*);
}
