// Validate APNG chunks, preserving real compressed pixels for the WIC codec.
#include "image_animation.h"
#include <cstring>
#include <limits>
namespace ruyijs::web_media {
namespace {
uint32_t BE(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
void Put(std::vector<uint8_t>& out,uint32_t n){for(int shift:{24,16,8,0})out.push_back(uint8_t(n>>shift));}
uint32_t CRC(const uint8_t* p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(unsigned b=0;b<8;b++)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}return ~c;}
void Chunk(std::vector<uint8_t>& out,const char* kind,const uint8_t* p,size_t n){
  Put(out,uint32_t(n));const size_t start=out.size();out.insert(out.end(),kind,kind+4);
  if(n)out.insert(out.end(),p,p+n);Put(out,CRC(out.data()+start,n+4));
}
}
bool ParseApng(const std::vector<uint8_t>& bytes,ImageAnimation* out) {
  static const uint8_t signature[]={137,80,78,71,13,10,26,10};
  if(bytes.size()<8)return true;
  if(memcmp(bytes.data(),signature,8))return false;
  std::vector<uint8_t> ihdr,shared,compressed;ImagePatch current;bool haveFrame=false,haveData=false;
  uint32_t sequence=0;size_t offset=8;
  auto finish=[&]() {
    if(!haveFrame||compressed.empty())return true;
    current.encoded.assign(signature,signature+8);auto header=ihdr;
    for(unsigned i=0;i<4;i++){header[i]=uint8_t(current.width>>(24-8*i));header[4+i]=uint8_t(current.height>>(24-8*i));}
    Chunk(current.encoded,"IHDR",header.data(),header.size());current.encoded.insert(current.encoded.end(),shared.begin(),shared.end());
    Chunk(current.encoded,"IDAT",compressed.data(),compressed.size());Chunk(current.encoded,"IEND",nullptr,0);
    out->patches.push_back(std::move(current));current=ImagePatch();compressed.clear();haveFrame=false;
    return out->patches.size()<=out->count;
  };
  while(offset+12<=bytes.size()) {
    const auto* chunk=bytes.data()+offset;const uint32_t length=BE(chunk);const auto* kind=chunk+4;const auto* p=chunk+8;
    if(length>domain::web_media::kEncodedImageLimit)return false;
    if(uint64_t(offset)+length+12>bytes.size())break;
    if(CRC(kind,size_t(length)+4)!=BE(p+length))return false;
    if(!memcmp(kind,"IHDR",4)) {
      if(offset!=8||length!=13)return false;ihdr.assign(p,p+length);out->width=BE(p);out->height=BE(p+4);
      if(!out->width||!out->height||uint64_t(out->width)*out->height>domain::web_media::kImagePixelLimit)return false;
    } else if(ihdr.empty())return false;
    else if(!memcmp(kind,"acTL",4)) {
      if(length!=8||out->recognized||haveData)return false;out->recognized=true;out->count=BE(p);
      if(!out->count||out->count>10000||uint64_t(out->width)*out->height*out->count>domain::web_media::kImagePixelLimit)return false;
      const auto loops=BE(p+4);out->repetitions=loops?double(loops-1):std::numeric_limits<double>::infinity();
    } else if(!memcmp(kind,"fcTL",4)) {
      if(!out->recognized||length!=26||BE(p)!=sequence++)return false;
      if(!finish())return false;haveFrame=true;
      current.width=BE(p+4);current.height=BE(p+8);current.x=BE(p+12);current.y=BE(p+16);
      if(!current.width||!current.height||uint64_t(current.x)+current.width>out->width||uint64_t(current.y)+current.height>out->height)return false;
      unsigned numerator=unsigned(p[20])<<8|p[21],denominator=unsigned(p[22])<<8|p[23];if(!denominator)denominator=100;
      const unsigned ms=uint32_t(uint64_t(numerator)*1000/denominator);current.duration=uint64_t(ms<=10?100:ms)*1000;
      current.disposal=p[24];current.over=p[25]==1;if(current.disposal>2||p[25]>1)return false;
    } else if(!memcmp(kind,"IDAT",4)) {
      haveData=true;if(haveFrame)compressed.insert(compressed.end(),p,p+length);
    } else if(!memcmp(kind,"fdAT",4)) {
      if(!haveFrame||length<4||BE(p)!=sequence++)return false;compressed.insert(compressed.end(),p+4,p+length);
    } else if(!memcmp(kind,"IEND",4)) {
      if(length||!finish())return false;out->complete=true;
      return !out->recognized||out->patches.size()==out->count;
    } else if(!haveData&&!haveFrame)shared.insert(shared.end(),chunk,chunk+length+12);
    offset+=size_t(length)+12;
  }
  return finish();
}
}
