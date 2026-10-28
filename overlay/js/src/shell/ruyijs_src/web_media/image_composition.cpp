// Shared WebMedia canvas composition; the codec supplies actual frame pixels.
#include "image_animation.h"
#include <algorithm>
#include <cstring>
namespace ruyijs::web_media {
#ifdef _WIN32
namespace {
bool Pixels(IWICImagingFactory* factory,const std::string& mime,const ImagePatch& patch,domain::web_media::ImageFrame* out) {
  wic::Pointer<IWICStream> stream;wic::Pointer<IWICBitmapDecoder> decoder;wic::Pointer<IWICBitmapFrameDecode> frame;
  if(FAILED(factory->CreateStream(stream.put()))||FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(patch.encoded.data()),DWORD(patch.encoded.size())))||
      FAILED(factory->CreateDecoder(*wic::Container(mime),nullptr,decoder.put()))||FAILED(decoder->Initialize(stream,WICDecodeMetadataCacheOnLoad))||
      FAILED(decoder->GetFrame(0,frame.put())))return false;
  return wic::PixelFrame(factory,frame,0,0,out);
}
bool Resize(IWICImagingFactory* factory,uint32_t w,uint32_t h,domain::web_media::ImageFrame* frame) {
  if(!w||!h||(w==frame->width&&h==frame->height))return true;
  if(uint64_t(w)*h>domain::web_media::kImagePixelLimit)return false;
  wic::Pointer<IWICBitmap> bitmap;wic::Pointer<IWICBitmapScaler> scaler;
  if(FAILED(factory->CreateBitmapFromMemory(frame->width,frame->height,GUID_WICPixelFormat32bppPBGRA,
      frame->width*4,UINT(frame->pixels.size()),frame->pixels.data(),bitmap.put()))||FAILED(factory->CreateBitmapScaler(scaler.put()))||
      FAILED(scaler->Initialize(bitmap,w,h,WICBitmapInterpolationModeFant)))return false;
  std::vector<uint8_t> pixels(size_t(w)*h*4);
  if(FAILED(scaler->CopyPixels(nullptr,w*4,UINT(pixels.size()),pixels.data())))return false;
  frame->width=w;frame->height=h;frame->pixels=std::move(pixels);return true;
}
}
bool ComposeImageAnimation(const std::string& mime,const ImageAnimation& input,uint32_t w,uint32_t h,domain::web_media::ImageData* out) {
  if(!input.width||!input.height)return false;
  wic::Apartment apartment;wic::Pointer<IWICImagingFactory> factory;
  if(!apartment.ok()||FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.put()))))return false;
  std::vector<uint8_t> canvas(size_t(input.width)*input.height*4,0),previous;uint64_t timestamp=0;bool alphaSeen=false;
  out->animated=true;out->repetitions=input.repetitions;
  for(const auto& patch:input.patches) {
    domain::web_media::ImageFrame image;if(!Pixels(factory,mime,patch,&image))return !input.complete;
    if(image.width!=patch.width||image.height!=patch.height)return false;
    if(patch.disposal==2)previous=canvas;
    for(uint32_t y=0;y<image.height;y++)for(uint32_t x=0;x<image.width;x++) {
      const auto* source=&image.pixels[(size_t(y)*image.width+x)*4];auto* target=&canvas[(size_t(y+patch.y)*input.width+x+patch.x)*4];
      const uint32_t alpha=source[3];
      for(unsigned c=0;c<4;c++) {
        const uint32_t value=c==3?alpha:(uint32_t(source[c])*alpha+254)/255;
        target[c]=uint8_t(patch.over?std::min(255u,value+(uint32_t(target[c])*(255-alpha)+127)/255):value);
      }
    }
    domain::web_media::ImageFrame complete;complete.width=input.width;complete.height=input.height;
    if(!image.opaque)alphaSeen=true;
    complete.pixels=canvas;complete.animated=true;complete.opaque=mime=="image/webp"&&!alphaSeen;
    if(complete.opaque)for(size_t i=3;i<canvas.size();i+=4)if(canvas[i]!=255){complete.opaque=false;break;}
    complete.timestamp=timestamp;complete.duration=patch.duration;timestamp+=patch.duration;
    if(!Resize(factory,w,h,&complete))return false;out->frames.push_back(std::move(complete));
    if(patch.disposal==1)for(uint32_t y=0;y<patch.height;y++)
      std::memset(&canvas[(size_t(y+patch.y)*input.width+patch.x)*4],0,size_t(patch.width)*4);
    else if(patch.disposal==2)canvas.swap(previous);
  }
  return true;
}

#else
bool ComposeImageAnimation(const std::string&, const ImageAnimation&, uint32_t, uint32_t,
                           domain::web_media::ImageData*) {
  return false;  // No WIC composition on this platform.
}
#endif
}
