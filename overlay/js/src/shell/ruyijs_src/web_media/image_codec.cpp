// Operating-system codec adapter. The domain sees encoded bytes and real pixels.
#include "web_media/image_wic.h"
#include "web_media/image_animation.h"
#include "web_media/image_gif_metadata.h"
#include <algorithm>
#include <limits>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace ruyijs::web_media {
namespace wic {
const GUID* Container(const std::string& mime) {
  if (mime == "image/png" || mime == "image/x-png") return &GUID_ContainerFormatPng;
  if (mime == "image/jpeg" || mime == "image/pjpeg" || mime == "image/jpg") return &GUID_ContainerFormatJpeg;
  if (mime == "image/gif") return &GUID_ContainerFormatGif;
  if (mime == "image/bmp" || mime == "image/x-ms-bmp") return &GUID_ContainerFormatBmp;
  if (mime == "image/x-icon" || mime == "image/vnd.microsoft.icon") return &GUID_ContainerFormatIco;
  if (mime == "image/webp") return &GUID_ContainerFormatWebp;
  return nullptr;
}
uint32_t Number(IWICMetadataQueryReader* reader, const wchar_t* name, uint32_t fallback) {
  if (!reader) return fallback;
  PROPVARIANT value; PropVariantInit(&value);
  if (FAILED(reader->GetMetadataByName(name, &value))) return fallback;
  const uint32_t result = value.vt == VT_UI1 ? value.bVal : value.vt == VT_UI2 ?
      value.uiVal : value.vt == VT_UI4 ? value.ulVal : value.vt == VT_BOOL ?
      unsigned(value.boolVal != VARIANT_FALSE) : fallback;
  PropVariantClear(&value); return result;
}
bool PixelFrame(IWICImagingFactory* factory, IWICBitmapFrameDecode* frame,
                uint32_t desiredWidth, uint32_t desiredHeight,
                domain::web_media::ImageFrame* out) {
  UINT width = 0, height = 0;
  if (FAILED(frame->GetSize(&width, &height)) || !width || !height) return false;
  Pointer<IWICBitmapScaler> scaler;
  IWICBitmapSource* source = frame;
  if (desiredWidth && desiredHeight && (width != desiredWidth || height != desiredHeight)) {
    if (FAILED(factory->CreateBitmapScaler(scaler.put())) ||
        FAILED(scaler->Initialize(frame, desiredWidth, desiredHeight, WICBitmapInterpolationModeFant))) return false;
    source = scaler; width = desiredWidth; height = desiredHeight;
  }
  if (uint64_t(width) * height > domain::web_media::kImagePixelLimit) return false;
  Pointer<IWICFormatConverter> converter;
  if (FAILED(factory->CreateFormatConverter(converter.put())) ||
      FAILED(converter->Initialize(source, GUID_WICPixelFormat32bppBGRA,
          WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return false;
  out->width = width; out->height = height;
  out->pixels.resize(size_t(width) * height * 4);
  if (FAILED(converter->CopyPixels(nullptr, width * 4, UINT(out->pixels.size()), out->pixels.data()))) return false;
  out->opaque = true;
  for (size_t i = 3; i < out->pixels.size(); i += 4) if (out->pixels[i] != 255) { out->opaque = false; break; }
  return true;
}
}
bool ImageCodecSupports(const std::string& type) {
  if (type == "image/avif") return true;  // Source-owned libavif/dav1d is linked.
  const GUID* container = wic::Container(type);
  if (!container) return false;
  wic::Apartment apartment;
  wic::Pointer<IWICImagingFactory> factory;
  wic::Pointer<IWICBitmapDecoder> decoder;
  return apartment.ok() && SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
      CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put()))) &&
      SUCCEEDED(factory->CreateDecoder(*container, nullptr, decoder.put()));
}
domain::web_media::ImageDecodeStatus DecodeImageBytes(const std::string& type,
    const std::vector<uint8_t>& bytes, uint32_t width, uint32_t height,
    bool noColor, domain::web_media::ImageData* output) {
  using Status = domain::web_media::ImageDecodeStatus;
  try {
  if (type == "image/avif") return DecodeAvifBytes(bytes, width, height, output);
  if(type=="image/png"||type=="image/x-png"||type=="image/webp") {
    ImageAnimation animation;
    const bool valid=type=="image/webp"?ParseWebpAnimation(bytes,&animation):ParseApng(bytes,&animation);
    if(!valid)return Status::InvalidData;
    if(animation.recognized)return animation.complete&&ComposeImageAnimation(type,animation,width,height,output)?Status::Ok:Status::InvalidData;
  }
  const GUID* expected = wic::Container(type);
  if (!expected) return Status::Unsupported;
  if (bytes.empty() || bytes.size() > domain::web_media::kEncodedImageLimit) return Status::InvalidData;
  wic::Apartment apartment;
  wic::Pointer<IWICImagingFactory> factory;
  wic::Pointer<IWICStream> stream;
  wic::Pointer<IWICBitmapDecoder> decoder;
  if (!apartment.ok() || FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
      CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())))) return Status::Unsupported;
  if (FAILED(factory->CreateDecoder(*expected, nullptr, decoder.put()))) return Status::Unsupported;
  if (FAILED(factory->CreateStream(stream.put())) ||
      FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()), DWORD(bytes.size()))) ||
      FAILED(decoder->Initialize(stream, WICDecodeMetadataCacheOnLoad))) return Status::InvalidData;
  UINT count = 0;
  if (FAILED(decoder->GetFrameCount(&count)) || !count || count > 10000) return Status::InvalidData;
  if(type=="image/gif") {
    GifMetadata metadata;if(!ReadGifMetadata(bytes,&metadata))return Status::InvalidData;
    if(count>1||metadata.animated)return wic::GifFrames(factory,decoder,width,height,output)?Status::Ok:Status::InvalidData;
  }
  wic::Pointer<IWICBitmapFrameDecode> frame;
  domain::web_media::ImageFrame pixels;
  if (FAILED(decoder->GetFrame(0, frame.put())) || !wic::PixelFrame(factory, frame, width, height, &pixels)) return Status::InvalidData;
  if (!noColor && (type == "image/bmp" || type == "image/x-ms-bmp")) ImageColorTransform(&pixels);
  output->frames.push_back(std::move(pixels));
  if (type != "image/png" && type != "image/x-png" && type != "image/gif") output->repetitions = std::numeric_limits<double>::infinity();
  for (auto& pixelFrame : output->frames) for (size_t i = 0; i < pixelFrame.pixels.size(); i += 4) {
    const uint32_t alpha = pixelFrame.pixels[i + 3];
    for (unsigned component = 0; component < 3; ++component)
      pixelFrame.pixels[i + component] = uint8_t((uint32_t(pixelFrame.pixels[i + component]) * alpha + 254) / 255);
  }
  return Status::Ok;
  } catch (const std::bad_alloc&) { return Status::OutOfMemory; }
}
}
