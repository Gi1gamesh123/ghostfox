// Match the browser's sRGB-to-display transform using the actual system profile.
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <fstream>
#endif
#include "web_media/image_codec.h"
#ifdef _WIN32
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"user32.lib")
#endif
extern "C" bool ruyijs_image_srgb_to_profile(uint8_t*,size_t,const uint8_t*,size_t);
namespace ruyijs::web_media {
void ImageColorTransform(domain::web_media::ImageFrame* frame) {
#ifdef _WIN32
  HDC dc=GetDC(nullptr);if(!dc)return;
  wchar_t path[32768];DWORD length=32768;
  const bool ok=GetICMProfileW(dc,&length,path)!=FALSE;ReleaseDC(nullptr,dc);
  if(!ok)return;
  std::ifstream input(path,std::ios::binary|std::ios::ate);if(!input)return;
  const auto lengthBytes=input.tellg();if(lengthBytes<=0||lengthBytes>16*1024*1024)return;
  std::vector<uint8_t> profile(size_t(lengthBytes),0);input.seekg(0);
  if(!input.read(reinterpret_cast<char*>(profile.data()),profile.size()))return;
  ruyijs_image_srgb_to_profile(frame->pixels.data(),frame->pixels.size(),profile.data(),profile.size());
#endif
}
}
