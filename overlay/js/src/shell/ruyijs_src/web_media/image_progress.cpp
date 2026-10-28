// Input EOF, available metadata and decoded frames are independent states.
#include "image_progress.h"
#include "web_media/image_animation.h"
#include "web_media/image_gif_metadata.h"
#include <new>
namespace ruyijs::web_media {
domain::web_media::ImageDecodeStatus DecodeImageProgress(const std::string& type,
    const std::vector<uint8_t>& bytes,uint32_t width,uint32_t height,bool noColor,
    bool eof,ImageProgress* out) {
  using Status=domain::web_media::ImageDecodeStatus;
  try {
    if(type=="image/png"||type=="image/x-png"||type=="image/webp") {
      ImageAnimation animation;
      const bool valid=type=="image/webp"?ParseWebpAnimation(bytes,&animation):ParseApng(bytes,&animation);
      if(!valid||(eof&&animation.recognized&&!animation.complete))return Status::InvalidData;
      if(animation.recognized) {
        if(!animation.count)return eof?Status::InvalidData:Status::Ok;
        out->metadata=true;out->frameCount=animation.count;
        out->frameCountFinal=type!="image/webp"||animation.complete;
        return ComposeImageAnimation(type,animation,width,height,&out->image)?Status::Ok:Status::InvalidData;
      }
    }
    const auto status=DecodeImageBytes(type,bytes,width,height,noColor,&out->image);
    if(status==Status::Ok){out->metadata=true;out->frameCountFinal=true;out->frameCount=uint32_t(out->image.frames.size());
      if(type=="image/gif") {
        GifMetadata metadata;if(!ReadGifMetadata(bytes,&metadata))return Status::InvalidData;
        out->frameCountFinal=metadata.complete;
        if(metadata.animated){out->image.animated=true;out->image.repetitions=metadata.repetitions;}
      }
    }
    if(!eof&&status==Status::InvalidData)return Status::Ok;
    return status;
  }catch(const std::bad_alloc&){return Status::OutOfMemory;}
}
}
