#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "web_media/image_codec.h"
#ifdef _WIN32
#include "web_media/image_wic.h"
#endif
namespace ruyijs::web_media {
struct ImagePatch {
  uint32_t x=0,y=0,width=0,height=0;
  uint64_t duration=0;
  unsigned disposal=0;
  bool over=false;
  std::vector<uint8_t> encoded;
};
struct ImageAnimation {
  uint32_t width=0,height=0,count=0;
  double repetitions=0;
  bool recognized=false,complete=false;
  std::vector<ImagePatch> patches;
};
bool ParseApng(const std::vector<uint8_t>&,ImageAnimation*);
bool ParseWebpAnimation(const std::vector<uint8_t>&,ImageAnimation*);
bool ComposeImageAnimation(const std::string&,const ImageAnimation&,uint32_t,uint32_t,
                          domain::web_media::ImageData*);
}
