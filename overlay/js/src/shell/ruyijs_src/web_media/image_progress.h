#pragma once
#include "web_media/image_codec.h"
namespace ruyijs::web_media {
struct ImageProgress {
  domain::web_media::ImageData image;
  uint32_t frameCount=0;
  bool metadata=false,frameCountFinal=false;
};
domain::web_media::ImageDecodeStatus DecodeImageProgress(const std::string&,
    const std::vector<uint8_t>&,uint32_t,uint32_t,bool,bool,ImageProgress*);
}
