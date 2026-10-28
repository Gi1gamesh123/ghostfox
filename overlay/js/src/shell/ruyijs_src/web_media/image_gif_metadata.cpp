// Container termination is determined structurally, never by a pixel decode.
#include "image_gif_metadata.h"
#include <cstring>
#include <limits>
namespace ruyijs::web_media {
bool ReadGifMetadata(const std::vector<uint8_t>& bytes,GifMetadata* out) {
  if(bytes.size()<13)return true;
  if(memcmp(bytes.data(),"GIF87a",6)&&memcmp(bytes.data(),"GIF89a",6))return false;
  size_t offset=13+(bytes[10]&128?size_t(3)<<(1+(bytes[10]&7)):0);
  if(offset>bytes.size())return true;
  while(offset<bytes.size()) {
    const uint8_t tag=bytes[offset++];
    if(tag==0x3b){out->complete=true;return true;}
    bool image=false;
    if(tag==0x2c) {
      if(offset+9>bytes.size())return true;image=true;
      const auto flags=bytes[offset+8];offset+=9+(flags&128?size_t(3)<<(1+(flags&7)):0);
      if(offset>=bytes.size())return true;offset++;
    } else if(tag==0x21) {
      if(offset>=bytes.size())return true;const auto label=bytes[offset++];
      if(label==0xff&&offset+17<=bytes.size()&&bytes[offset]==11&&
          (!memcmp(bytes.data()+offset+1,"NETSCAPE2.0",11)||!memcmp(bytes.data()+offset+1,"ANIMEXTS1.0",11))&&bytes[offset+12]==3&&bytes[offset+13]==1) {
        const unsigned loops=unsigned(bytes[offset+14])|unsigned(bytes[offset+15])<<8;
        out->repetitions=loops?double(loops):std::numeric_limits<double>::infinity();out->animated=true;
      }
    } else return false;
    while(true) {
      if(offset>=bytes.size())return true;const auto length=bytes[offset++];
      if(!length)break;if(offset+length>bytes.size())return true;offset+=length;
    }
    if(image)out->frames++;
  }
  return true;
}
}
