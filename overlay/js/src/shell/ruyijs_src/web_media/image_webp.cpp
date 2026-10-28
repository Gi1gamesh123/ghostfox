// ANIM/ANMF retain their encoded VP8/VP8L pixels; WIC decodes each real frame.
#include "image_animation.h"
#include <cstring>
#include <limits>
namespace ruyijs::web_media {
namespace {
uint32_t LE(const uint8_t* p,unsigned n){uint32_t v=0;for(unsigned i=0;i<n;i++)v|=uint32_t(p[i])<<(i*8);return v;}
void Put(std::vector<uint8_t>& out,uint32_t n,unsigned size){for(unsigned i=0;i<size;i++)out.push_back(uint8_t(n>>(i*8)));}
}
bool ParseWebpAnimation(const std::vector<uint8_t>& bytes,ImageAnimation* out) {
  if(bytes.size()<12)return true;
  if(memcmp(bytes.data(),"RIFF",4)||memcmp(bytes.data()+8,"WEBP",4))return false;
  const uint64_t end=uint64_t(LE(bytes.data()+4,4))+8;
  if(end>domain::web_media::kEncodedImageLimit||end<12)return false;
  size_t offset=12;
  while(offset+8<=bytes.size()&&offset+8<=end) {
    const auto* kind=bytes.data()+offset;const uint32_t length=LE(kind+4,4);const auto* p=kind+8;
    const uint64_t next=uint64_t(offset)+8+length+(length&1);if(next>end)return false;if(next>bytes.size())break;
    if(!memcmp(kind,"VP8X",4)) {
      if(length!=10)return false;out->width=LE(p+4,3)+1;out->height=LE(p+7,3)+1;
      out->recognized=bool(p[0]&2);if(uint64_t(out->width)*out->height>domain::web_media::kImagePixelLimit)return false;
    } else if(!memcmp(kind,"ANIM",4)) {
      if(!out->recognized||length!=6)return false;unsigned loops=LE(p+4,2);
      out->repetitions=loops?double(loops-1):std::numeric_limits<double>::infinity();
    } else if(!memcmp(kind,"ANMF",4)) {
      if(!out->recognized||length<16)return false;ImagePatch frame;
      frame.x=LE(p,3)*2;frame.y=LE(p+3,3)*2;frame.width=LE(p+6,3)+1;frame.height=LE(p+9,3)+1;
      if(uint64_t(frame.x)+frame.width>out->width||uint64_t(frame.y)+frame.height>out->height)return false;
      unsigned ms=LE(p+12,3);frame.duration=uint64_t(ms<=10?100:ms)*1000;frame.disposal=p[15]&1;frame.over=!(p[15]&2);
      if(p[15]&~3u)return false;
      auto& encoded=frame.encoded;encoded.insert(encoded.end(),{'R','I','F','F'});Put(encoded,4+18+length-16,4);
      encoded.insert(encoded.end(),{'W','E','B','P','V','P','8','X'});Put(encoded,10,4);Put(encoded,16,4);
      Put(encoded,frame.width-1,3);Put(encoded,frame.height-1,3);encoded.insert(encoded.end(),p+16,p+length);
      out->patches.push_back(std::move(frame));out->count=uint32_t(out->patches.size());
      if(out->count>10000||uint64_t(out->width)*out->height*out->count>domain::web_media::kImagePixelLimit)return false;
    }
    offset=size_t(next);
  }
  out->complete=offset==end;return true;
}
}
