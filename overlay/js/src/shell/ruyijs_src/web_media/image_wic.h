#pragma once
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include "web_media/image_codec.h"

namespace ruyijs::web_media::wic {
template<class T> struct Pointer {
  T* value = nullptr;
  Pointer() = default;
  Pointer(const Pointer&) = delete;
  Pointer& operator=(const Pointer&) = delete;
  ~Pointer() { if (value) value->Release(); }
  T** put() { return &value; }
  T* operator->() const { return value; }
  operator T*() const { return value; }
};
struct Apartment {
  HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  ~Apartment() { if (SUCCEEDED(result)) CoUninitialize(); }
  bool ok() const { return SUCCEEDED(result) || result == RPC_E_CHANGED_MODE; }
};
const GUID* Container(const std::string&);
uint32_t Number(IWICMetadataQueryReader*, const wchar_t*, uint32_t = 0);
bool PixelFrame(IWICImagingFactory*, IWICBitmapFrameDecode*, uint32_t, uint32_t,
                domain::web_media::ImageFrame*);
bool GifFrames(IWICImagingFactory*, IWICBitmapDecoder*, uint32_t, uint32_t,
               domain::web_media::ImageData*);
}
